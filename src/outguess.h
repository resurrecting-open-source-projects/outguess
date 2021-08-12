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

#ifndef _OUTGUESS_H
#define _OUTGUESS_H

#include "arc.h"

#define BITSHIFT	0	/* which bit in the byte the data is in */

#define MAX_DEPTH 3             /* maximum number of bytes per pixel */
#define MAX_SEEK	1024	/* maximum number of pixels for foil stats */

#define STEG_EMBED	0x01
#define STEG_MARK	0x02
#define STEG_ERROR	0x08

#define STEG_RETRIEVE	0x10

#define STEG_STATS	0x20

extern int steg_stat;

/*
 * The generic bitmap structure.  An object is passed in and an object
 * dependant function extracts all bits that can be modified to embed
 * data into a bitmap structure.  This allows the embedding be independant
 * of the carrier data.
 */

typedef struct _bitmap {
	u_char *bitmap;		/* the bitmap */
	u_char *locked;		/* bits that may not be modified */
	u_char *metalock;	/* bits that have been used for foil */
	char *detect;		/* relative detectability of changes */
	char *data;		/* data associated with the bit */
	int bytes;		/* allocated bytes */
	int bits;		/* number of bits in here */

				/* function to call for preserve stats */
	int (*preserve)(struct _bitmap *, int);
	size_t maxcorrect;
} bitmap;

#define STEG_ERR_HEADER		1
#define STEG_ERR_BODY		2
#define STEG_ERR_PERM		3	/* error independant of seed */

typedef struct _stegres {
	int error;		/* Errors during steg embed */
	int changed;		/* Number of changed bits in data */
	int bias;		/* Accumulated bias of changed bits */
} stegres;

typedef struct _config {
	int flags;
	int siter;
	int siterstart;
} config;

#define TEST_BIT(x,y)		((x)[(y) / 8] & (1 << ((y) & 7)))
#define WRITE_BIT(x,y,what)	((x)[(y) / 8] = ((x)[(y) / 8] & \
				~(1 << ((y) & 7))) | ((what) << ((y) & 7)))

#define SWAP(x,y)		do {int n = x; x = y; y = n;} while(0);

void *checkedmalloc(size_t n);

u_char *encode_data(u_char *, int *, struct arc4_stream *, int);
u_char *decode_data(u_char *, int *, struct arc4_stream *, int);

struct _iterator;

stegres steg_embed(bitmap *bitmap, struct _iterator *iter,
		   struct arc4_stream *as, u_char *data, u_int datalen,
		   u_int16_t seed, int embed);
u_int32_t steg_retrbyte(bitmap *bitmap, int bits, struct _iterator *iter);

char *steg_retrieve(int *len, bitmap *bitmap, struct _iterator *iter,
		    struct arc4_stream *as, int);

void mmap_file(char *name, u_char **data, int *size);
void munmap_file(u_char *data, int len);

#endif /* _OUTGUESS_H */
