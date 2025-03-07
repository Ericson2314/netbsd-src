/*	$NetBSD: externs.h,v 1.21 2021/08/31 17:22:24 rillig Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * inittyp.c
 */
extern	void	inittyp(void);

/*
 * tyname.c
 */
extern	const char *type_name(const type_t *);
extern	const	char *tspec_name(tspec_t);

/*
 * mem.c
 */
extern	size_t	mem_block_size(void);
extern	void	*xmalloc(size_t);
extern	void	*xcalloc(size_t, size_t);
extern	void	*xrealloc(void *, size_t);
extern	char	*xstrdup(const char *);
extern  char	*xasprintf(const char *, ...) __printflike(1, 2);

/*
 * emit.c
 */
#if defined(IS_LINT1) || defined(IS_LINT2)
extern	ob_t	ob;

extern	void	outopen(const char *);
extern	void	outclose(void);
extern	void	outclr(void);
extern	void	outchar(int);
extern	void	outqchar(int);
extern	void	outstrg(const char *);
extern	void	outint(int);
#define outname(a)	outname1(__FILE__, __LINE__, a);
extern	void	outname1(const char *, size_t, const char *);
extern	void	outsrc(const char *);
#endif
