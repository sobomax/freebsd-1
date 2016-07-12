/* LINTLIBRARY */
/*-
 * Copyright 1996-1998 John D. Polstra.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dlfcn.h>
#include <stdlib.h>

#include "libc_private.h"
#include "crt_private.h"
#include "crtbrand.c"
#include "ignore_init.c"

typedef void (*fptr)(void);

#ifdef GCRT
extern void _mcleanup(void);
extern void monstartup(void *, void *);
extern int eprol;
extern int etext;
#endif

void _start(char **, void (*)(void));

/* The entry function. */
void
_start(char **ap, void (*cleanup)(void))
{
	int argc;
	char **argv;
	char **env;
	main_t main_over;

	argc = *(long *)(void *)ap;
	argv = ap + 1;
	env = ap + 2 + argc;
	handle_argv(argc, argv, env);

	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	else
		_init_tls();

#ifdef GCRT
	atexit(_mcleanup);
	monstartup(&eprol, &etext);
__asm__("eprol:");
#endif

	handle_static_init(argc, argv, env);
	if (&_DYNAMIC != NULL) {
		main_over = (main_t)dlfunc(RTLD_NEXT, "main");
		if (main_over != NULL) {
			exit(main_over(argc, argv, env));
		}
	}

	exit(main(argc, argv, env));
}
