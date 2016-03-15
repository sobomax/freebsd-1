/*-
 * Copyright (c) 2010 George V. Neville-Neil <gnn@freebsd.org>
 * Copyright (c) 2015 Adrian Chadd <adrian@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/pmc_mdep.h>

#define	MIPS74K_PMC_CAPS	(PMC_CAP_INTERRUPT | PMC_CAP_USER |     \
				 PMC_CAP_SYSTEM | PMC_CAP_EDGE |	\
				 PMC_CAP_THRESHOLD | PMC_CAP_READ |	\
				 PMC_CAP_WRITE | PMC_CAP_INVERT |	\
				 PMC_CAP_QUALIFIER)

/* 0x1 - Exception_enable */
#define MIPS74K_PMC_INTERRUPT_ENABLE      0x10 /* Enable interrupts */
#define MIPS74K_PMC_USER_ENABLE           0x08 /* Count in USER mode */
#define MIPS74K_PMC_SUPER_ENABLE          0x04 /* Count in SUPERVISOR mode */
#define MIPS74K_PMC_KERNEL_ENABLE         0x02 /* Count in KERNEL mode */
#define MIPS74K_PMC_ENABLE (MIPS74K_PMC_USER_ENABLE |	   \
			    MIPS74K_PMC_SUPER_ENABLE |	   \
			    MIPS74K_PMC_KERNEL_ENABLE)

#define MIPS74K_PMC_SELECT 5 /* Which bit position the event starts at. */

const struct mips_event_code_map mips_event_codes[] = {
	{ PMC_EV_MIPS74K_CYCLES, MIPS_CTR_ALL, 0 },
	{ PMC_EV_MIPS74K_INSTR_EXECUTED, MIPS_CTR_ALL, 1 },
	{ PMC_EV_MIPS74K_PREDICTED_JR_31, MIPS_CTR_0, 2 },
	{ PMC_EV_MIPS74K_JR_31_MISPREDICTIONS, MIPS_CTR_1, 2 },
	{ PMC_EV_MIPS74K_REDIRECT_STALLS, MIPS_CTR_0, 3 },
	{ PMC_EV_MIPS74K_JR_31_NO_PREDICTIONS, MIPS_CTR_1, 3 },
	{ PMC_EV_MIPS74K_ITLB_ACCESSES, MIPS_CTR_0, 4 },
	{ PMC_EV_MIPS74K_ITLB_MISSES, MIPS_CTR_1, 4 },
	{ PMC_EV_MIPS74K_JTLB_INSN_MISSES, MIPS_CTR_1, 5 },
	{ PMC_EV_MIPS74K_ICACHE_ACCESSES, MIPS_CTR_0, 6 },
	{ PMC_EV_MIPS74K_ICACHE_MISSES, MIPS_CTR_1, 6 },
	{ PMC_EV_MIPS74K_ICACHE_MISS_STALLS, MIPS_CTR_0, 7 },
	{ PMC_EV_MIPS74K_UNCACHED_IFETCH_STALLS, MIPS_CTR_0, 8 },
	{ PMC_EV_MIPS74K_PDTRACE_BACK_STALLS, MIPS_CTR_1, 8 },
	{ PMC_EV_MIPS74K_IFU_REPLAYS, MIPS_CTR_0, 9 },
	{ PMC_EV_MIPS74K_KILLED_FETCH_SLOTS, MIPS_CTR_1, 9 },
	{ PMC_EV_MIPS74K_IFU_IDU_MISS_PRED_UPSTREAM_CYCLES, MIPS_CTR_0, 11 },
	{ PMC_EV_MIPS74K_IFU_IDU_NO_FETCH_CYCLES, MIPS_CTR_1, 11 },
	{ PMC_EV_MIPS74K_IFU_IDU_CLOGED_DOWNSTREAM_CYCLES, MIPS_CTR_0, 12 },
	{ PMC_EV_MIPS74K_DDQ0_FULL_DR_STALLS, MIPS_CTR_0, 13 },
	{ PMC_EV_MIPS74K_DDQ1_FULL_DR_STALLS, MIPS_CTR_1, 13 },
	{ PMC_EV_MIPS74K_ALCB_FULL_DR_STALLS, MIPS_CTR_0, 14 },
	{ PMC_EV_MIPS74K_AGCB_FULL_DR_STALLS, MIPS_CTR_1, 14 },
	{ PMC_EV_MIPS74K_CLDQ_FULL_DR_STALLS, MIPS_CTR_0, 15 },
	{ PMC_EV_MIPS74K_IODQ_FULL_DR_STALLS, MIPS_CTR_1, 15 },
	{ PMC_EV_MIPS74K_ALU_EMPTY_CYCLES, MIPS_CTR_0, 16 },
	{ PMC_EV_MIPS74K_AGEN_EMPTY_CYCLES, MIPS_CTR_1, 16 },
	{ PMC_EV_MIPS74K_ALU_OPERANDS_NOT_READY_CYCLES, MIPS_CTR_0, 17 },
	{ PMC_EV_MIPS74K_AGEN_OPERANDS_NOT_READY_CYCLES, MIPS_CTR_1, 17 },
	{ PMC_EV_MIPS74K_ALU_NO_ISSUES_CYCLES, MIPS_CTR_0, 18 },
	{ PMC_EV_MIPS74K_AGEN_NO_ISSUES_CYCLES, MIPS_CTR_1, 18 },
	{ PMC_EV_MIPS74K_ALU_BUBBLE_CYCLES, MIPS_CTR_0, 19 },
	{ PMC_EV_MIPS74K_AGEN_BUBBLE_CYCLES, MIPS_CTR_1, 19 },
	{ PMC_EV_MIPS74K_SINGLE_ISSUE_CYCLES, MIPS_CTR_0, 20 },
	{ PMC_EV_MIPS74K_DUAL_ISSUE_CYCLES, MIPS_CTR_1, 20 },
	{ PMC_EV_MIPS74K_OOO_ALU_ISSUE_CYCLES, MIPS_CTR_0, 21 },
	{ PMC_EV_MIPS74K_OOO_AGEN_ISSUE_CYCLES, MIPS_CTR_1, 21 },
	{ PMC_EV_MIPS74K_JALR_JALR_HB_INSNS, MIPS_CTR_0, 22 },
	{ PMC_EV_MIPS74K_DCACHE_LINE_REFILL_REQUESTS, MIPS_CTR_1, 22 },
	{ PMC_EV_MIPS74K_DCACHE_LOAD_ACCESSES, MIPS_CTR_0, 23 },
	{ PMC_EV_MIPS74K_DCACHE_ACCESSES, MIPS_CTR_1, 23 },
	{ PMC_EV_MIPS74K_DCACHE_WRITEBACKS, MIPS_CTR_0, 24 },
	{ PMC_EV_MIPS74K_DCACHE_MISSES, MIPS_CTR_1, 24 },
	{ PMC_EV_MIPS74K_JTLB_DATA_ACCESSES, MIPS_CTR_0, 25 },
	{ PMC_EV_MIPS74K_JTLB_DATA_MISSES, MIPS_CTR_1, 25 },
	{ PMC_EV_MIPS74K_LOAD_STORE_REPLAYS, MIPS_CTR_0, 26 },
	{ PMC_EV_MIPS74K_VA_TRANSALTION_CORNER_CASES, MIPS_CTR_1, 26 },
	{ PMC_EV_MIPS74K_LOAD_STORE_BLOCKED_CYCLES, MIPS_CTR_0, 27 },
	{ PMC_EV_MIPS74K_LOAD_STORE_NO_FILL_REQUESTS, MIPS_CTR_1, 27 },
	{ PMC_EV_MIPS74K_L2_CACHE_WRITEBACKS, MIPS_CTR_0, 28 },
	{ PMC_EV_MIPS74K_L2_CACHE_ACCESSES, MIPS_CTR_1, 28 },
	{ PMC_EV_MIPS74K_L2_CACHE_MISSES, MIPS_CTR_0, 29 },
	{ PMC_EV_MIPS74K_L2_CACHE_MISS_CYCLES, MIPS_CTR_1, 29 },
	{ PMC_EV_MIPS74K_FSB_FULL_STALLS, MIPS_CTR_0, 30 },
	{ PMC_EV_MIPS74K_FSB_OVER_50_FULL, MIPS_CTR_1, 30 },
	{ PMC_EV_MIPS74K_LDQ_FULL_STALLS, MIPS_CTR_0, 31 },
	{ PMC_EV_MIPS74K_LDQ_OVER_50_FULL, MIPS_CTR_1, 31 },
	{ PMC_EV_MIPS74K_WBB_FULL_STALLS, MIPS_CTR_0, 32 },
	{ PMC_EV_MIPS74K_WBB_OVER_50_FULL, MIPS_CTR_1, 32 },
	{ PMC_EV_MIPS74K_LOAD_MISS_CONSUMER_REPLAYS, MIPS_CTR_0, 35 },
	{ PMC_EV_MIPS74K_CP1_CP2_LOAD_INSNS, MIPS_CTR_1, 35 },
	{ PMC_EV_MIPS74K_JR_NON_31_INSNS, MIPS_CTR_0, 36 },
	{ PMC_EV_MIPS74K_MISPREDICTED_JR_31_INSNS, MIPS_CTR_1, 36 },
	{ PMC_EV_MIPS74K_BRANCH_INSNS, MIPS_CTR_0, 37 },
	{ PMC_EV_MIPS74K_CP1_CP2_COND_BRANCH_INSNS, MIPS_CTR_1, 37 },
	{ PMC_EV_MIPS74K_BRANCH_LIKELY_INSNS, MIPS_CTR_0, 38 },
	{ PMC_EV_MIPS74K_MISPREDICTED_BRANCH_LIKELY_INSNS, MIPS_CTR_1, 38 },
	{ PMC_EV_MIPS74K_COND_BRANCH_INSNS, MIPS_CTR_0, 39 },
	{ PMC_EV_MIPS74K_MISPREDICTED_BRANCH_INSNS, MIPS_CTR_1, 39 },
	{ PMC_EV_MIPS74K_INTEGER_INSNS, MIPS_CTR_0, 40 },
	{ PMC_EV_MIPS74K_FPU_INSNS, MIPS_CTR_1, 40 },
	{ PMC_EV_MIPS74K_LOAD_INSNS, MIPS_CTR_0, 41 },
	{ PMC_EV_MIPS74K_STORE_INSNS, MIPS_CTR_1, 41 },
	{ PMC_EV_MIPS74K_J_JAL_INSNS, MIPS_CTR_0, 42 },
	{ PMC_EV_MIPS74K_MIPS16_INSNS, MIPS_CTR_1, 42 },
	{ PMC_EV_MIPS74K_NOP_INSNS, MIPS_CTR_0, 43 },
	{ PMC_EV_MIPS74K_NT_MUL_DIV_INSNS, MIPS_CTR_1, 43 },
	{ PMC_EV_MIPS74K_DSP_INSNS, MIPS_CTR_0, 44 },
	{ PMC_EV_MIPS74K_ALU_DSP_SATURATION_INSNS, MIPS_CTR_1, 44 },
	{ PMC_EV_MIPS74K_DSP_BRANCH_INSNS, MIPS_CTR_0, 45 },
	{ PMC_EV_MIPS74K_MDU_DSP_SATURATION_INSNS, MIPS_CTR_1, 45 },
	{ PMC_EV_MIPS74K_UNCACHED_LOAD_INSNS, MIPS_CTR_0, 46 },
	{ PMC_EV_MIPS74K_UNCACHED_STORE_INSNS, MIPS_CTR_1, 46 },
	{ PMC_EV_MIPS74K_EJTAG_INSN_TRIGGERS, MIPS_CTR_0, 49 },
	{ PMC_EV_MIPS74K_CP1_BRANCH_MISPREDICTIONS, MIPS_CTR_0, 50 },
	{ PMC_EV_MIPS74K_SC_INSNS, MIPS_CTR_0, 51 },
	{ PMC_EV_MIPS74K_FAILED_SC_INSNS, MIPS_CTR_1, 51 },
	{ PMC_EV_MIPS74K_PREFETCH_INSNS, MIPS_CTR_0, 52 },
	{ PMC_EV_MIPS74K_CACHE_HIT_PREFETCH_INSNS, MIPS_CTR_1, 52 },
	{ PMC_EV_MIPS74K_NO_INSN_CYCLES, MIPS_CTR_0, 53 },
	{ PMC_EV_MIPS74K_LOAD_MISS_INSNS, MIPS_CTR_1, 53 },
	{ PMC_EV_MIPS74K_ONE_INSN_CYCLES, MIPS_CTR_0, 54 },
	{ PMC_EV_MIPS74K_TWO_INSNS_CYCLES, MIPS_CTR_1, 54 },
	{ PMC_EV_MIPS74K_GFIFO_BLOCKED_CYCLES, MIPS_CTR_0, 55 },
	{ PMC_EV_MIPS74K_CP1_CP2_STORE_INSNS, MIPS_CTR_1, 55 },
	{ PMC_EV_MIPS74K_MISPREDICTION_STALLS, MIPS_CTR_0, 56 },
	{ PMC_EV_MIPS74K_MISPREDICTED_BRANCH_INSNS_CYCLES, MIPS_CTR_0, 57 },
	{ PMC_EV_MIPS74K_EXCEPTIONS_TAKEN, MIPS_CTR_0, 58 },
	{ PMC_EV_MIPS74K_GRADUATION_REPLAYS, MIPS_CTR_1, 58 },
	{ PMC_EV_MIPS74K_COREEXTEND_EVENTS, MIPS_CTR_0, 59 },
	{ PMC_EV_MIPS74K_ISPRAM_EVENTS, MIPS_CTR_0, 62 },
	{ PMC_EV_MIPS74K_DSPRAM_EVENTS, MIPS_CTR_1, 62 },
	{ PMC_EV_MIPS74K_L2_CACHE_SINGLE_BIT_ERRORS, MIPS_CTR_0, 63 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_0, MIPS_CTR_0, 64 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_1, MIPS_CTR_1, 64 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_2, MIPS_CTR_0, 65 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_3, MIPS_CTR_1, 65 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_4, MIPS_CTR_0, 66 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_5, MIPS_CTR_1, 66 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_6, MIPS_CTR_0, 67 },
	{ PMC_EV_MIPS74K_SYSTEM_EVENT_7, MIPS_CTR_1, 67 },
	{ PMC_EV_MIPS74K_OCP_ALL_REQUESTS, MIPS_CTR_0, 68 },
	{ PMC_EV_MIPS74K_OCP_ALL_CACHEABLE_REQUESTS, MIPS_CTR_1, 68 },
	{ PMC_EV_MIPS74K_OCP_READ_REQUESTS, MIPS_CTR_0, 69 },
	{ PMC_EV_MIPS74K_OCP_READ_CACHEABLE_REQUESTS, MIPS_CTR_1, 69 },
	{ PMC_EV_MIPS74K_OCP_WRITE_REQUESTS, MIPS_CTR_0, 70 },
	{ PMC_EV_MIPS74K_OCP_WRITE_CACHEABLE_REQUESTS, MIPS_CTR_1, 70 },
	{ PMC_EV_MIPS74K_FSB_LESS_25_FULL, MIPS_CTR_0, 74 },
	{ PMC_EV_MIPS74K_FSB_25_50_FULL, MIPS_CTR_1, 74 },
	{ PMC_EV_MIPS74K_LDQ_LESS_25_FULL, MIPS_CTR_0, 75 },
	{ PMC_EV_MIPS74K_LDQ_25_50_FULL, MIPS_CTR_1, 75 },
	{ PMC_EV_MIPS74K_WBB_LESS_25_FULL, MIPS_CTR_0, 76 },
	{ PMC_EV_MIPS74K_WBB_25_50_FULL, MIPS_CTR_1, 76 },
};

const int mips_event_codes_size =
	sizeof(mips_event_codes) / sizeof(mips_event_codes[0]);

struct mips_pmc_spec mips_pmc_spec = {
	.ps_cpuclass = PMC_CLASS_MIPS74K,
	.ps_cputype = PMC_CPU_MIPS_74K,
	.ps_capabilities = MIPS74K_PMC_CAPS,
	.ps_counter_width = 32
};

/*
 * Performance Count Register N
 */
uint64_t
mips_pmcn_read(unsigned int pmc)
{
	uint32_t reg = 0;

	KASSERT(pmc < mips_npmcs, ("[mips74k,%d] illegal PMC number %d",
				   __LINE__, pmc));

	/* The counter value is the next value after the control register. */
	switch (pmc) {
	case 0:
		reg = mips_rd_perfcnt1();
		break;
	case 1:
		reg = mips_rd_perfcnt3();
		break;
	default:
		return 0;
	}
	return (reg);
}

uint64_t
mips_pmcn_write(unsigned int pmc, uint64_t reg)
{

	KASSERT(pmc < mips_npmcs, ("[mips74k,%d] illegal PMC number %d",
				   __LINE__, pmc));

	switch (pmc) {
	case 0:
		mips_wr_perfcnt0(reg);
		break;
	case 1:
		mips_wr_perfcnt1(reg);
		break;
	case 2:
		mips_wr_perfcnt2(reg);
		break;
	case 3:
		mips_wr_perfcnt3(reg);
		break;
	default:
		return 0;
	}
	return (reg);
}

uint32_t
mips_get_perfctl(int cpu, int ri, uint32_t event, uint32_t caps)
{
	uint32_t config;

	config = event;

	config <<= MIPS74K_PMC_SELECT;

	if (caps & PMC_CAP_SYSTEM)
		config |= (MIPS74K_PMC_SUPER_ENABLE |
			   MIPS74K_PMC_KERNEL_ENABLE);
	if (caps & PMC_CAP_USER)
		config |= MIPS74K_PMC_USER_ENABLE;
	if ((caps & (PMC_CAP_USER | PMC_CAP_SYSTEM)) == 0)
		config |= MIPS74K_PMC_ENABLE;
	if (caps & PMC_CAP_INTERRUPT)
		config |= MIPS74K_PMC_INTERRUPT_ENABLE;

	PMCDBG2(MDP,ALL,2,"mips74k-get_perfctl ri=%d -> config=0x%x", ri, config);

	return (config);
}
