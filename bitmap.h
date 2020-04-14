/*
 * Copyright (C) 1999-2001, Niels Provos <provos@citi.umich.edu>
 * Copyright (C) 2020, Robin Vobruba <hoijui.quaero@gmail.com>
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

#ifndef _BITMAP_H
#define _BITMAP_H

#include <stddef.h>
#include <stdint.h>

/*
 * The generic bitmap structure.  An object is passed in and an object
 * dependant function extracts all bits that can be modified to embed
 * data into a bitmap structure.  This allows the embedding be independant
 * of the carrier data.
 */
typedef struct _bitmap {
	char *bitmap;		/* the bitmap */
	char *locked;		/* bits that may not be modified */
	char *metalock;	/* bits that have been used for foil */
	char *detect;		/* relative detectability of changes */
	char *data;		/* data associated with the bit */
	size_t bytes;		/* allocated bytes */
	size_t bits;		/* number of bits in here */

	/* function to call for preserve stats */
	int (*preserve)(struct _bitmap *, int);
	size_t maxcorrect;
} bitmap;

#endif /* _BITMAP_H */
