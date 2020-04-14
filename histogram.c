/*
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

#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "config.h"
#include "outguess.h"

char *progname;

void
usage(void)
{

	fprintf(stderr, "%s: <file>\n", progname);
}

void
histogram_simple(char *data, int bits)
{
	int i;
	int one = 0, zero = 0;

	for (i = 0; i < bits; i++)
		if (TEST_BIT(data, i))
			one++;
		else
			zero++;

	fprintf(stdout, "Bits: %6d\n", bits);
	fprintf(stdout, "One:  %6d, %f\n", one, (float)one/bits);
	fprintf(stdout, "Zero: %6d, %f\n", zero, (float)zero/bits);
}

#define MAXRUNLEN	25

void
histogram_runlen(char *data, int bits)
{
	int buckets[MAXRUNLEN];
	int what, count, i;

	memset(buckets, 0, sizeof(buckets));
	what = TEST_BIT(data, 0);
	count = 1;
	for (i = 1; i < bits; i++) {
		if (TEST_BIT(data, i) != what) {
			if (count >= MAXRUNLEN)
				count = MAXRUNLEN - 1;
			buckets[count]++;

			what = TEST_BIT(data, i);
			count = 1;
		} else
			count++;
	}
	if (count >= MAXRUNLEN)
		count = MAXRUNLEN - 1;
	buckets[count]++;

	for (i = 1; i < MAXRUNLEN; i++) {
		fprintf(stdout, "%3d:  %6d, %f\n",
			i, buckets[i], (float)buckets[i]/bits);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fin;
	char *data;
	int bits, bytes, res;

	progname = argv[0];

	argv++;
	argc--;

	if (argc != 1) {
		usage();
		exit(1);
	}

	if ((fin = fopen(argv[0], "r")) == NULL)
		err(1, "fopen");

	if ((res = fread(&bits, sizeof(int), 1, fin)) != 1)
		err(1, "fread(1): %d", res);

	bits = ntohl(bits);
	bytes = (bits + 7) / 8;
	if ((data = malloc(bytes)) == NULL)
		err(1, "malloc");

	if ((res = fread(data, sizeof(char), bytes, fin)) != bytes)
		err(1, "fread(2): %d", res);

	histogram_simple(data, bits);
	histogram_runlen(data, bits);

	exit(0);
}
