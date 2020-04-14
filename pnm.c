/*
 * Copyright (C) 1999-2001, Niels Provos <provos@citi.umich.edu>
 * Copyright (C) 2016, Chris Rorvick <chris@rorvick.com>
 * Copyright (C) 2016, Joao Eriberto Mota Filho <eriberto@debian.org>
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

/* This source code is derived in parts from
 *   StirMark -- Experimental watermark resilience testkit
 *   Markus Kuhn <mkuhn@acm.org>, University of Cambridge
 *   Copyright (c) 1997 Markus Kuhn <mkuhn@acm.org>
 */

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "config.h"
#include "outguess.h"
#include "pnm.h"

/* The functions that can be used to handle a PNM data object */

handler pnm_handler = {
	"ppm",
	init_pnm,
	read_pnm,
	write_pnm,
	bitmap_from_pnm,
	bitmap_to_pnm,
	preserve_pnm,
};

void
init_pnm(char *parameter)
{
}

int
preserve_pnm(bitmap *bitmap, int off)
{
	if (off == -1)
		bitmap->preserve = preserve_pnm;

	return (-1);
}

/* skip whitespace and comments in PGM/PPM headers */
void
skip_white(FILE *f)
{
	int c;

	do {
		while (isspace(c = getc(f)));
		if (c == '#')
			while ((c = getc(f)) != '\n' && c != EOF);
		else {
			ungetc(c, f);
			return;
		}
	} while (c != EOF);
}

void
bitmap_to_pnm(image *image, bitmap *bitmap, int flags)
{
	uint8_t *img = image->img;

	size_t off = 0;
	for (size_t i = 0; i < bitmap->bits; ) {
		uint8_t tmp = bitmap->bitmap[off++];
		for (size_t j = 0; j < 8 && i < bitmap->bits; j++) {
			if ((flags & STEG_MARK) && TEST_BIT(bitmap->locked, i))
				img[i] = 255;

			img[i] = (img[i] & ~(1 << BITSHIFT)) |
				((tmp & 1) << BITSHIFT);
			i++;
			tmp >>= 1;
		}
	}
}

void
bitmap_from_pnm(bitmap *bitmap, image *image, int flags)
{
	size_t off;
	uint8_t *img;
	int x, y, depth;

	img = image->img;
	x = image->x;
	y = image->y;
	depth = image->depth;

	memset(bitmap, 0, sizeof(*bitmap));

	bitmap->bits = x * y * depth;
	bitmap->bytes = (bitmap->bits + 7) / 8;
	bitmap->bitmap = checkedmalloc(bitmap->bytes);
	bitmap->locked = checkedmalloc(bitmap->bytes);
	bitmap->metalock = checkedmalloc(bitmap->bytes);
	bitmap->detect = checkedmalloc(bitmap->bits);
	bitmap->data = checkedmalloc(bitmap->bits);

	memset (bitmap->locked, 0, bitmap->bytes);

	off = 0;
	for (size_t i = 0; i < bitmap->bits; ) {
		uint8_t tmp = 0;
		for (size_t j = 0; j < 8 && i < bitmap->bits; j++) {
			/* Weight image modifications */
			if (img[i] >= PNM_THRES_MAX)
				bitmap->detect[i] = -1;
			else if (img[i] <= PNM_THRES_MIN)
				bitmap->detect[i] = 1;
			else
				bitmap->detect[i] = 0;

			bitmap->data[i] = img[i];

			tmp |= ((img[i++] & (1 << BITSHIFT)) >> BITSHIFT) << j;
		}
		bitmap->bitmap[off++] = tmp;
	}
}


image *
read_pnm(FILE *fin)
{
	image *image;
	char magic[10];
	int v;

	image = checkedmalloc(sizeof(*image));
	memset(image, 0, sizeof(*image));

	const char* const getsRet = fgets(magic, 10, fin);
	if (getsRet == NULL) {
		fprintf(stderr, "Failed to read the magic value of the image!\n");
		fprintf(stderr, "This suggest either an I/O error, ");
		fprintf(stderr, "or that the file is invalid.\n");
		exit(1);
	}
	if (magic[0] != 'P' || !isdigit(magic[1]) || magic[2] != '\n') {
		fprintf(stderr, "Unsupported input file type!\n");
		exit(1);
	}
	skip_white(fin);
	int nScanned = fscanf(fin, "%d", &image->x);
	if (nScanned != 1) {
		fprintf(stderr, "Failed to read image width!\n");
		exit(1);
	}
	skip_white(fin);
	nScanned = fscanf(fin, "%d", &image->y);
	if (nScanned != 1) {
		fprintf(stderr, "Failed to read image height!\n");
		exit(1);
	}
	skip_white(fin);
	nScanned = fscanf(fin, "%d", &image->max);
	if (nScanned != 1) {
		fprintf(stderr, "Failed to read image max pixel value!\n");
		exit(1);
	}
	getc(fin);
	if (image->max > 255 || image->max <= 0 || image->x <= 1 ||
			image->y <= 1)
	{
		fprintf(stderr, "Unsupported value range!\n");
		exit(1);
	}

	switch (magic[1]) {
	case '2': /* PGM ASCII */
	case '5': /* PGM binary */
		image->depth = 1; /* up to 8 bit/pixel */
		break;
	case '3': /* PPM ASCII */
	case '6': /* PPM binary */
		image->depth = 3; /* up to 24 bit/pixel */
		break;
	default:
		fprintf(stderr, "Unsupported input file type 'P%c'!\n", magic[1]);
		exit(1);
	}

	image->img = (unsigned char *) checkedmalloc(sizeof(unsigned char) *
			image->x * image->y *
			image->depth);

	switch (magic[1]) {
	case '2': /* PGM ASCII */
	case '3': /* PPM ASCII */
		for (size_t i = 0; i < image->x * image->y * image->depth; i++) {
			skip_white(fin);
			nScanned = fscanf(fin, "%d", &v);
			if (nScanned != 1) {
				fprintf(stderr, "Failed to read image pixel value!\n");
				exit(1);
			}
			if (v < 0 || v > image->max) {
				fprintf(stderr, "Out of range value %d!\n", v);
				exit(1);
			}
			(image->img)[i] = v;
		}
		break;
	case '5': /* PGM binary */
	case '6': /* PPM binary */
		if (fread(image->img, image->x * image->depth, image->y, fin)
				!= image->y)
		{
			fprintf(stderr, "Failed to read PPM image data!\n");
			fprintf(stderr, "This suggest either an I/O error, ");
			fprintf(stderr, "or that the file is invalid.\n");
			exit(1);
		}
		break;
	}

	if (ferror(fin)) {
		perror("Error occurred while reading input file");
		exit(1);
	}
	if (feof(fin)) {
		fprintf(stderr, "Unexpected end of input file!\n");
		exit(1);
	}

	return image;
}

void
write_pnm(FILE *fout, image *image)
{
	fprintf(fout, "P%d\n%d %d\n%d\n", image->depth == 1 ? 5 : 6,
		image->x, image->y, image->max);

	fwrite(image->img, image->x*image->y*image->depth, sizeof(uint8_t),
			fout);
}

void
free_pnm(image *image)
{
	free(image->img);
	free(image);
}
