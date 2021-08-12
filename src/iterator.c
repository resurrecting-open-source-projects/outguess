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

#include <sys/types.h>

#include "config.h"

#include "outguess.h"
#include "arc.h"
#include "iterator.h"

/* Initalize the iterator */

void
iterator_init(iterator *iter, bitmap *bitmap, u_char *key, u_int klen)
{
	iter->skipmod = INIT_SKIPMOD;

	arc4_initkey(&iter->as, "Seeding", key, klen);

	iter->off = arc4_getword(&iter->as) % iter->skipmod;
}

/* The next bit in the bitmap we should embed data into */

int
iterator_next(iterator *iter, bitmap *bitmap)
{
	iter->off += (arc4_getword(&iter->as) % iter->skipmod) + 1;

	return iter->off;
}

void
iterator_seed(iterator *iter, bitmap *bitmap, u_int16_t seed)
{
	u_int8_t reseed[2];

	reseed[0] = seed;
	reseed[1] = seed >> 8;

	arc4_addrandom(&iter->as, reseed, 2);
}

void
iterator_adapt(iterator *iter, bitmap *bitmap, int datalen)
{
	iter->skipmod = SKIPADJ(bitmap->bits, bitmap->bits - iter->off) *
		(bitmap->bits - iter->off)/(8 * datalen);
}
