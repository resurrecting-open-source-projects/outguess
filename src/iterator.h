/*
 * Copyright (C) 1999-2001 Niels Provos <provos@citi.umich.edu>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
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

#ifndef _ITERATOR_H
#define _ITERATOR_H

#define INIT_SKIPMOD	32
#define DEFAULT_ITER	256

/* Slowly fade skip out at the end of the picture */
#define SKIPADJ(x,y)	((y) > (x)/32 ? 2 : 2 - ((x/32) - (y))/(float)(x/32))

/*
 * The generic iterator
 */

typedef struct _iterator {
	struct arc4_stream as;
	u_int32_t skipmod;
	int off;		/* Current bit position */
} iterator;

struct _bitmap;

void iterator_init(iterator *, struct _bitmap *,  u_char *key, u_int klen);
int iterator_next(iterator *, struct _bitmap *);

#define ITERATOR_CURRENT(x)	(x)->off

void iterator_seed(iterator *, struct _bitmap *, u_int16_t);
void iterator_adapt(iterator *, struct _bitmap *, int);

#endif
