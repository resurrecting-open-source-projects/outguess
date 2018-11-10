/*
 * Outguess - a Universal Steganograpy Tool for
 * Steganographic Experiments and Fourier Transformation
.* (c) 1999 Niels Provos <provos@citi.umich.edu>
 * Features
 * - multiple data embedding
 * - PRNG driven selection of bits
 * - error correcting encoding
 * - modular architecture for different selection and embedding algorithms
 */

/*
 * Copyright 1999 Niels Provos <provos@citi.umich.edu>
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
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "config.h"

#include "arc.h"
#include "outguess.h"
#include "golay.h"
#include "pnm.h"
#include "jpg.h"

#ifdef FOURIER
#include <math.h>
#include <rfftw.h>

void fft_visible(int xdim, int ydim, fftw_complex *c, u_char *img,
		 double maxre, double maxim, double maxmod);
fftw_complex *fft_transform(int xdim, int ydim, unsigned char *data,
	       double *mre, double *mim, double *mmod);
void fft_filter(int xdim, int ydim, fftw_complex *data);
u_char *fft_transform_back(int xdim, int ydim, fftw_complex *data);

#endif /* FOURIER */

#ifndef MAP_FAILED
/* Some Linux systems are missing this */
#define MAP_FAILED	(void *)-1
#endif /* MAP_FAILED */

static int steg_err_buf[CODEBITS];
static int steg_err_cnt;
static int steg_errors;
static int steg_encoded;

static int steg_count;
static int steg_mis;
static int steg_mod;

/* Exported variables */

int steg_stat;

/* format handlers */

handler *handlers[] = {
	&pnm_handler,
	&jpg_handler
};

handler *
get_handler(char *name)
{
	int i;
	
	if (!(name = strrchr(name, '.')))
		return NULL;
	name++;

	for (i = sizeof(handlers)/sizeof(handler *) - 1; i >= 0; i--)
		if (!strcasecmp(name, handlers[i]->extension))
			return handlers[i];

	return NULL;
}

void *
checkedmalloc(size_t n)
{
	void *p;
  
	if (!(p = malloc(n))) {
		fprintf(stderr, "checkedmalloc: not enough memory\n");
		exit(1);
	}
  
	return p;
}

#ifdef FOURIER

/* if depth > 1 */
int
split_colors(u_char **pred, u_char **pgreen, u_char **pblue, 
	     u_char *img,
	     int xdim, int ydim, int depth)
{
	int i, j;
	u_char *red, *green, *blue;

	/* Split to red - blue */
	red = checkedmalloc(ydim*xdim);
	green = checkedmalloc(ydim*xdim);
	blue = checkedmalloc(ydim*xdim);

	for (i = 0; i < ydim; i++) {
		for (j = 0; j < xdim; j++) {
			red[j + xdim*i] = img[0 + depth*(j + xdim*i)];
			green[j + xdim*i] = img[1 + depth*(j + xdim*i)];
			blue[j + xdim*i] = img[2 + depth*(j + xdim*i)];
		}
	}

	*pred = red;
	*pgreen = green;
	*pblue = blue;

	return 1;
}

void
fft_image(int xdim, int ydim, int depth, u_char *img)
{
	int i,j;
	u_char *red, *green, *blue;
	fftw_complex *c, *d, *e;
	double maxre, maxim, maxmod;

	split_colors(&red, &green, &blue, img, xdim, ydim, depth);
  
	maxre = maxim = maxmod = 0;
	c = fft_transform(xdim, ydim, red, &maxre, &maxim, &maxmod);
	d = fft_transform(xdim, ydim, green, &maxre, &maxim, &maxmod);
	e = fft_transform(xdim, ydim, blue, &maxre, &maxim, &maxmod);
	fft_visible(xdim, ydim, c, red, maxre, maxim, maxmod);
	free(c);
	fft_visible(xdim, ydim, d, green, maxre, maxim, maxmod);
	free(d);
	fft_visible(xdim, ydim, e, blue, maxre, maxim, maxmod);
	free(e);

	for (i = 0; i < ydim; i++) {
		for (j = 0; j < xdim; j++) {
			img[0 + depth*(j + xdim*i)] = red[j + xdim*i];
			img[1 + depth*(j + xdim*i)] = green[j + xdim*i];
			img[2 + depth*(j + xdim*i)] = blue[j + xdim*i];
		}
	}

	free(red); free(green); free(blue);
}

void
fft_visible(int xdim, int ydim, fftw_complex *c, u_char *img,
	    double maxre, double maxim, double maxmod)
{
	int i, j, ind;
	double contrast, scale, factor, total, val;

	contrast = exp((-100.5) * log(xdim*ydim) / 256);
	factor = - 255.0;
	total = 0;
	scale = factor / log(maxmod * contrast * contrast + 1.0);

	printf("Visible: max (%f/%f/%f), contrast: %f, scale: %f\n",
	       maxre, maxim, maxmod, contrast, scale);

	for (i = 0; i < xdim ; i++)
		for (j = 0; j < ydim; j++) {
			ind = j*xdim + i;
			val = total - scale*log(contrast*contrast*
						(c[ind].re*c[ind].re +
						 c[ind].im*c[ind].im) + 1);
			img[ind] = val;
		}
}

fftw_complex *
fft_transform(int xdim, int ydim, unsigned char *data,
	       double *mre, double *mim, double *mmod)
{
	rfftwnd_plan p;
	int i,j, ind, di, dj;
	double maxre, maxim, maxmod, val;
	fftw_complex *a;
  
	fprintf(stderr, "Starting complex 2d FFT\n");

	a = checkedmalloc(xdim * ydim * sizeof(fftw_complex));

	p = fftw2d_create_plan(ydim, xdim, FFTW_FORWARD,
			       FFTW_ESTIMATE | FFTW_IN_PLACE);

	di = 1;
	for (j = 0; j < ydim; j++) {
		dj = di;
		for (i = 0; i < xdim ; i++) {
			ind = i + j * xdim;
			a[ind].re = data[ind] * dj;
			a[ind].im = 0.0;
			dj = -dj;
		}
		di = -di;
	}

	fftwnd_one(p, a, NULL);

	maxim = *mim;
	maxre = *mre;
	maxmod = *mmod;
	for (i = 0; i < xdim ; i++)
		for (j = 0; j < ydim; j++) {
			ind = j*xdim + i;

			/* Update Stats */
			if (fabs(a[ind].re) > maxre) maxre = fabs(a[ind].re);
			if (fabs(a[ind].im) > maxim) maxim = fabs(a[ind].im);
			val = a[ind].re * a[ind].re + a[ind].im * a[ind].im;
			if (val > maxmod) maxmod = val;
		}
	*mim = maxim;
	*mre = maxre;
	*mmod = maxmod;

	fftwnd_destroy_plan(p);

	return a;
}

void
fft_filter(int xdim, int ydim, fftw_complex *data)
{
	int i, j, ind;
	double val;

	fprintf(stderr, "Starting complex filtering\n");

	for (j = 0; j < ydim; j++) {
		for (i = 0; i < xdim ; i++) {
			ind = i + j * xdim;
			val = sqrt((xdim/2-i)*(xdim/2-i) +
				   (ydim/2-j)*(ydim/2-j));
			if (val > 15) {
				data[ind].re = 2*data[ind].re;
				data[ind].im = 2*data[ind].im;
			} else {
				data[ind].re = 0.2*data[ind].re;
				data[ind].im = 0.2*data[ind].im;
			}
		}
	}
}

u_char *
fft_transform_back(int xdim, int ydim, fftw_complex *data)
{
	rfftwnd_plan p;
	int i,j, ind;
	fftw_real dj, val;
	u_char *a;

	fprintf(stderr, "Starting complex 2d FFT-back\n");

	a = checkedmalloc(xdim * ydim * sizeof(u_char));

	p = fftw2d_create_plan(ydim, xdim, FFTW_BACKWARD,
			       FFTW_ESTIMATE | FFTW_IN_PLACE);

	fftwnd_one(p, data, NULL);

	dj = (xdim * ydim);
	for (j = 0; j < ydim; j++) {
		for (i = 0; i < xdim ; i++) {
			ind = i + j * xdim;
			val = sqrt(data[ind].re * data[ind].re +
				   data[ind].im * data[ind].im)/dj;
			a[ind] = val;
		}
	}

	return a;
}
#endif /* FOURIER */

/* Initalize the iterator */

void
iterator_init(iterator *iter, bitmap *bitmap, struct arc4_stream *as)
{
	int i;
	char derive[16];

	iter->skipmod = INIT_SKIPMOD;
	iter->as = *as;

	/* Put the PRNG in different state, using key dependant data
	 * provided by the PRNG itself.
	 */
	for (i = 0; i < sizeof(derive); i++)
		derive[i] = arc4_getbyte(&iter->as);
	arc4_addrandom(&iter->as, derive, sizeof(derive));

	iter->off = arc4_getword(&iter->as) % iter->skipmod;
}

/* The next bit in the bitmap we should embed data into */

int
iterator_next(iterator *iter, bitmap *bitmap)
{
	iter->off += (arc4_getword(&iter->as) % iter->skipmod) + 1;

	return iter->off;
}

static __inline int
iterator_current(iterator *iter)
{
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

/*
 * The error correction might allow us to introduce extra errors to
 * avoid modifying data.  Choose to leave bits with high detectability
 * untouched.
 */

void
steg_adjust_errors(bitmap *bitmap, int flags)
{
	int i, j, n, many, flag; 
	int priority[ERRORBITS], detect[ERRORBITS];

	many = ERRORBITS - steg_errors;
	for (j = 0; j < many && j < steg_err_cnt; j++) {
		priority[j] = steg_err_buf[j];
		detect[j] = bitmap->detect[priority[j]];
	}

	/* Very simple sort */
	do {
		for (flag = 0, i = 0; i < j - 1; i++)
			if (detect[i] < detect[i + 1]) {
				SWAP(detect[i], detect[i+1]);
				SWAP(priority[i], priority[i+1]);
				flag = 1;
			}
	} while (flag);

	for (i = j; i < steg_err_cnt; i++) {
		for (n = 0; n < j; n++)
			if (detect[n] < bitmap->detect[steg_err_buf[i]])
				break;
		if (n < j - 1) {
			memmove(detect + n + 1, detect + n,
				(j - n) * sizeof(int));
			memmove(priority + n + 1, priority + n,
				(j - n) * sizeof(int));
		}
		if (n < j) {
			priority[n] = steg_err_buf[i];
			detect[n] = bitmap->detect[steg_err_buf[i]];
		}
	}

	for (i = 0; i < j; i++) {
		if (flags & STEG_EMBED) {
			if (TEST_BIT(bitmap->bitmap, priority[i]))
				WRITE_BIT(bitmap->bitmap, i, 0);
			else
				WRITE_BIT(bitmap->bitmap, i, 1);
		}
		steg_mis--;
		steg_mod -= detect[i];
	}
}

int
steg_embedchunk(bitmap *bitmap, iterator *iter,
		u_int32_t data, int bits, int embed)
{
	int i = iterator_current(iter);
	u_int32_t val;

	while (i < bitmap->bits && bits) {
		if ((embed & STEG_ERROR) && !steg_encoded) {
			if (steg_err_cnt > 0)
				steg_adjust_errors(bitmap, embed);
			steg_encoded = CODEBITS;
			steg_errors = 0;
			steg_err_cnt = 0;
			memset(steg_err_buf, 0, sizeof(steg_err_buf));
		}
		steg_encoded--;

		val = (TEST_BIT(bitmap->bitmap, i) ? 1 : 0) ^ (data & 1);
		steg_count++;
		if (val == 1) {
			steg_mod += bitmap->detect[i];
			steg_mis++;
		}

		/* Check if we are allowed to change a bit here */
		if ((embed & STEG_FORBID) && (val == 1) &&
		    (TEST_BIT(bitmap->locked, i))) {
			if (!(embed & STEG_ERROR) || (++steg_errors > 3))
				return 0;
			val = 2;
		}

		/* Store the bits we changed in error encoding mode */
		if ((embed & STEG_ERROR) && val == 1)
			steg_err_buf[steg_err_cnt++] = i;

		if (val != 2 && (embed & STEG_EMBED)) {
			if (embed & (STEG_FORBID|STEG_MARK))
				WRITE_BIT(bitmap->locked, i, 1);
      
			WRITE_BIT(bitmap->bitmap, i, data & 1);
		}

		data >>= 1;
		bits--;

		i = iterator_next(iter, bitmap);
	}

	return 1;
}

stegres
steg_embed(bitmap *bitmap, iterator *iter, struct arc4_stream *as,
	   u_char *data, u_int datalen, u_int16_t seed, int embed)
{
	int i, len;
	u_int32_t tmp = 0;
	u_char tmpbuf[4], *encbuf;
	stegres result;

	steg_count = steg_mis = steg_mod = 0;

	memset(&result, 0, sizeof(result));

	if (bitmap->bits / (datalen * 8) < 2) {
		fprintf(stderr, "steb_embed: not enough bits in bitmap "
			"for embedding: %d > %d/2\n",
			datalen * 8, bitmap->bits);
		exit (1);
	}

	if (embed & STEG_EMBED)
		fprintf(stderr, "Embedding data: %d in %d\n",
			datalen * 8, bitmap->bits);

	/* Clear error counter */
	steg_encoded = 0;
	steg_err_cnt = 0;

	/* Encode the seed and datalen */
	tmpbuf[0] = seed & 0xff;
	tmpbuf[1] = seed >> 8;
	tmpbuf[2] = datalen & 0xff;
	tmpbuf[3] = datalen >> 8;
	
	/* Encode the admin data XXX maybe derive another stream */
	len = 4;
	encbuf = encode_data (tmpbuf, &len, as, embed);

	for (i = 0; i < len; i++)
		if (!steg_embedchunk(bitmap, iter, encbuf[i], 8, embed)) {
			free (encbuf);
			
			/* If we use error correction or a bit in the seed
			 * was locked, we can go on, otherwise we have to fail.
			 */
			if ((embed & STEG_ERROR) ||
			    steg_count < 16 /* XXX */)
				result.error = STEG_ERR_HEADER;
			else
				result.error = STEG_ERR_PERM;
			return result;
		}
	free (encbuf);

	/* Clear error counter again, a new ECC block starts */
	steg_encoded = 0;

	iterator_seed(iter, bitmap, seed);

	while (iterator_current(iter) < bitmap->bits && datalen > 0) {
		iterator_adapt(iter, bitmap, datalen);
		
		tmp = *data++;
		datalen--;

		if (!steg_embedchunk(bitmap, iter, tmp, 8, embed)) {
			result.error = STEG_ERR_BODY;
			return result;
		}
	}

	/* Final error adjustion after end */
	if ((embed & STEG_ERROR) && steg_err_cnt > 0)
	  steg_adjust_errors(bitmap, embed);

	if (embed & STEG_EMBED)
		fprintf(stderr, "Bits embedded: %d, changed: %d(%2.1f%%), "
			"bias: %d, tot: %d, skip: %d\n",
			steg_count, steg_mis,
			(float) 100 * steg_mis/steg_count,
			steg_mod,
			iterator_current(iter),
			iterator_current(iter) - steg_count);

	result.changed = steg_mis;
	result.bias = steg_mod;

	return result;
}

u_int32_t
steg_retrbyte(bitmap *bitmap, int bits, iterator *iter)
{
	u_int32_t i = iterator_current(iter);
	int where;
	u_int32_t tmp = 0;

	for (where = 0; where < bits; where++) {
		tmp |= (TEST_BIT(bitmap->bitmap, i) ? 1 : 0) << where;

		i = iterator_next(iter, bitmap);
	}

	return tmp;
}

char *
steg_retrieve(int *len, bitmap *bitmap, iterator *iter, struct arc4_stream *as,
	      int flags)
{
	u_int32_t n;
	int i;
	u_int32_t origlen;
	u_int16_t seed;
	u_int datalen;
	u_char *buf;
	u_int8_t *tmpbuf;


	datalen = 4;
	encode_data(NULL, &datalen, NULL, flags);
	tmpbuf = checkedmalloc(datalen);

	for (i = 0; i < datalen; i++)
		tmpbuf[i] = steg_retrbyte(bitmap, 8, iter);

	buf = decode_data (tmpbuf, &datalen, as, flags);

	if (datalen != 4) {
		fprintf (stderr, "Steg retrieve: wrong data len: %d\n",
			 datalen);
		exit (1);
	}

	free (tmpbuf);

	seed = buf[0] | (buf[1] << 8);
	origlen = datalen = buf[2] | (buf[3] << 8);

	free (buf);

	fprintf(stderr, "Steg retrieve: seed: %d, len: %d\n", seed, datalen);

	if (datalen > bitmap->bytes) {
		fprintf(stderr, "Extracted datalen is too long: %d > %d\n",
			datalen, bitmap->bytes);
		exit(1);
	}

	buf = checkedmalloc(datalen);

	iterator_seed(iter, bitmap, seed);

	n = 0;
	while (datalen > 0) {
		iterator_adapt(iter, bitmap, datalen);
		buf[n++] = steg_retrbyte(bitmap, 8, iter);
		datalen --;
	}

	*len = origlen;
	return buf;
}

int
steg_find(bitmap *bitmap, iterator *iter, struct arc4_stream *as,
	  int siter, int siterstart,
	  u_char *data, int datalen, int flags)
{
	int changed, tch, half, chmax, chmin;
	int j, i, size = 0;
	struct arc4_stream tas;
	iterator titer;
	u_int16_t *chstats = NULL;
	stegres result;

	half = datalen * 8 / 2;
	
	if (!siter && !siterstart)
		siter = DEFAULT_ITER;

	if (siter && siterstart < siter) {
		if (steg_stat) {
			/* Collect stats about changed bit */
			size = siter - siterstart;
			chstats = checkedmalloc(size * sizeof(u_int16_t));
			memset(chstats, 0, size * sizeof(u_int16_t));
		}

		fprintf(stderr, "Finding best embedding...\n");
		changed = chmin = chmax = -1; j = -STEG_ERR_HEADER;

		for (i = siterstart; i < siter; i++) {
			titer = *iter;
			tas = *as;
			result = steg_embed(bitmap, &titer, &tas,
					 data, datalen, i, flags);
			/* Seed does not effect any more */
			if (result.error == STEG_ERR_PERM)
				return -result.error;
			else if (result.error)
				continue;

			tch = result.changed + result.bias;

			if (steg_stat)
				chstats[i - siterstart] = result.changed;

			if (chmax == -1 || result.changed > chmax)
				chmax = result.changed;
			if (chmin == -1 || result.changed < chmin)
				chmin = result.changed;

			if (changed == -1 || tch < changed) {
				changed = tch;
				j = i;
				fprintf(stderr, "New best: %5u: %5u(%2.1f%%), bias %5u(%1.2f), saved: % 5d\n",
					j, result.changed, 
					(float) 100 * steg_mis / steg_count,
					result.bias, 
					(float)result.bias / steg_mis,
					(half - result.changed) / 8);
			}
		}

		if (steg_stat && (chmax - chmin > 1)) {
			double mean = 0, dev, sq;
			int cnt = 0, count = chmax - chmin + 1;
			u_int16_t *chtab;
			int chtabcnt = 0;

			chtab = checkedmalloc(count * sizeof(u_int16_t));
			memset(chtab, 0, count * sizeof(u_int16_t));

			for (i = 0; i < size; i++)
				if (chstats[i] > 0) {
					mean += chstats[i];
					cnt++;
					chtab[chstats[i] - chmin]++;
					chtabcnt++;
				}

			mean = mean / cnt;
			dev = 0;
			for (i = 0; i < size; i++)
				if (chstats[i] > 0) {
					sq = chstats[i] - mean;
					dev += sq * sq;
				}

			fprintf(stderr, "Changed bits. Min: %d, Mean: %f, +- %f, Max: %d\n",
				chmin,
				mean, sqrt(dev / (cnt - 1)),
				chmax);

			if (steg_stat > 1)
				for (i = 0; i < count; i++) {
					if (!chtab[i])
						continue;
					fprintf(stderr, "%d: %.9f\n",
						chmin + i,
						(double)chtab[i]/chtabcnt);
				}

			free (chtab);
			free (chstats);
		}

		fprintf(stderr, "%d, %d: ", j, changed);
	} else
		j = siterstart;

	return j;
}

/* graphic file handling routines */

u_char *
encode_data(u_char *data, int *len, struct arc4_stream *as, int flags)
{
	int j, datalen = *len;
	u_char *encdata;

	if (flags & STEG_ERROR) {
		int eclen, i = 0, length = 0;
		u_int32_t tmp;
		u_int64_t code = 0;
		u_char edata[3];

		datalen = datalen + (3 - (datalen % 3));
		eclen = (datalen * 8 / DATABITS * CODEBITS + 7)/ 8;

		if (data == NULL) {
			*len = eclen;
			return NULL;
		}

		encdata = checkedmalloc(3 * eclen * sizeof(u_char));
		while (datalen > 0) {
			if (datalen > 3)
				memcpy(edata, data, 3);
			else {
				int adj = *len % 3;
				memcpy (edata, data, adj);

				/* Self describing padding */
				for (j = 2; j >= adj; j--)
					edata[j] = j - adj;
			}
			tmp = edata[0];
			tmp |= edata[1] << 8;
			tmp |= edata[2] << 16;

			data += 3;
			datalen -= 3;

			for (j = 0; j < 2; j++) {
				code |= ENCODE(tmp & DATAMASK) << length;
				length += CODEBITS;
				while (length >= 8) {
					encdata[i++] = code & 0xff;
					code >>= 8;
					length -= 8;
				}
				tmp >>= DATABITS;
			}
		}
    
		/* Encode the rest */
		if (length > 0)
			encdata[i++] = code & 0xff;

		datalen = eclen;
		data = encdata;
	} else {
		if (data == NULL) {
			*len = datalen;
			return NULL;
		}
		encdata = checkedmalloc(datalen * sizeof(u_char));
	}

	/* Encryption */
	for (j = 0; j < datalen; j++)
		encdata[j] = data[j] ^ arc4_getbyte(as);

	*len = datalen;

	return encdata;
}

u_char *
decode_data(u_char *encdata, int *len, struct arc4_stream *as, int flags)
{
	int i, j, enclen = *len, declen;
	u_char *data;

	for (j = 0; j < enclen; j++)
		encdata[j] = encdata[j] ^ arc4_getbyte(as);
  
	if (flags & STEG_ERROR) {
		u_int32_t inbits = 0, outbits = 0, etmp, dtmp;

		declen = enclen * DATABITS / CODEBITS;
		data = checkedmalloc(declen * sizeof(u_char));

		etmp = dtmp = 0;
		for (i = 0, j = 0; i < enclen && j < declen; ) {
			while (outbits < CODEBITS) {
				etmp |= TDECODE(encdata + i, enclen)<< outbits;
				i++;
				outbits += 8;
			}
			dtmp |= (DECODE(etmp & CODEMASK) >>
				 (CODEBITS - DATABITS)) << inbits;
			inbits += DATABITS;
			etmp >>= CODEBITS;
			outbits -= CODEBITS;
			while (inbits >= 8) {
				data[j++] = dtmp & 0xff;
				dtmp >>= 8;
				inbits -= 8;
			}
		}

		i = data[declen -1];
		if (i > 2) {
			fprintf (stderr, "decode_data: padding is incorrect: %d\n",
				 i);
			*len = 0;
			return data;
		}
		for (j = i; j >= 0; j--)
			if (data[declen - 1 - i + j] != j)
				break;
		if (j >= 0) {
			fprintf (stderr, "decode_data: padding is incorrect: %d\n",
				 i);
			*len = 0;
			return data;
		}

		declen -= i + 1;
		fprintf (stderr, "Decode: %d data after ECC: %d\n",
			 *len, declen);

	} else {
		data = checkedmalloc(enclen * sizeof(u_char));
		declen = enclen;
		memcpy (data, encdata, declen);
	}

	*len = declen;
	return data;
}

void
mmap_file(char *name, u_char **data, int *size)
{
	int fd;
	struct stat fs;
	char *p;

	if ((fd = open(name, O_RDONLY, 0)) == -1) {
		fprintf(stderr, "Can not open %s\n", name);
		exit(1);
	}

	if (fstat(fd, &fs) == -1) {
		perror("fstat");
		exit(1);
	}

#ifdef HAVE_MMAP
	if ((p = mmap(NULL, fs.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
#else
	p = checkedmalloc(fs.st_size);
	if (read(fd, p, fs.st_size) != fs.st_size) {
		perror("read");
		exit(1);
	}
#endif /* HAVE_MMAP */
	close(fd);

	*data = p;
	*size = fs.st_size;
}

void
munmap_file(u_char *data, int len)
{
#ifdef HAVE_MMAP
	if (munmap(data, len) == -1) {
		perror("munmap");
		exit(1);
	}
#else
	free (data);
#endif /* HAVE_MMAP */
}

int
main(int argc, char **argv)
{
	char version[] = "OutGuess 0.13b Universal Stego (c) 1999 Niels Provos";
	char usage[] = "%s\n\n%s [options] [<input file> [<output file>]]\n"
		"\t-[sS] <n>    iteration start, capital letter for 2nd dataset\n"
		"\t-[iI] <n>    iteration limit\n"
		"\t-[kK] <key>  key\n"
		"\t-[dD] <name> filename of dataset\n"
		"\t-[eE]        use error correcting encoding\n"
		"\t-p <param>   parameter passed to destination data handler\n"
		"\t-r           retrieve message from data\n"
		"\t-x <n>       number of key derivations to be tried\n"
		"\t-m           mark pixels that have been modified\n"
		"\t-t           collect statistic information\n"
#ifdef FOURIER
		"\t-f           fourier transform modified image\n"
#endif /* FOURIER */
		;

	FILE *fin = stdin, *fout = stdout;
	image *image;
	handler *srch = NULL, *dsth = NULL;
	char *param = NULL;
	unsigned char *encdata;
	bitmap bitmap;	/* Extracted bits that we may modify */
	iterator iter, titer;
	int j, ch, derive = 0;
	stegres cumres, tmpres;
	u_int16_t siter = 0, siterstart = 0, siter2 = 0, siterstart2 = 0;
	u_char *data = NULL, *data2 = NULL;
	int enclen, datalen, flags = 0;
	char *key = "Default key", *key2 = NULL;
	struct arc4_stream as, tas;
	char mark = 0, useforbidden = 0, doretrieve = 0;
	char doerror = 0, doerror2 = 0;
#ifdef FOURIER
	char dofourier = 0;
#endif /* FOURIER */

	steg_stat = 0;

	/* read command line arguments */
	while ((ch = getopt(argc, argv, "eErmftp:s:S:i:I:k:d:D:K:x:")) != -1)
		switch((char)ch) {
		case 'k':
			key = optarg;
			break;
		case 'K':
			key2 = optarg;
			useforbidden = 1;
			break;
		case 'p':
			param = optarg;
			break;
		case 'x':
			derive = atoi(optarg);
			break;
		case 'i':
			siter = atoi(optarg);
			break;
		case 'I':
			siter2 = atoi(optarg);
			break;
		case 'r':
			doretrieve = 1;
			break;
		case 't':
			steg_stat++;
			break;
		case 's':
			siterstart = atoi(optarg);
			break;
		case 'S':
			siterstart2 = atoi(optarg);
			break;
#ifdef FOURIER
		case 'f':
			dofourier = 1;
			break;
#endif /* FOURIER */
		case 'm':
			mark = 1; /* Mark bytes we modified with 255 */
			break;
		case 'd':
			data = optarg;
			break;
		case 'D':
			data2 = optarg;
			break;
		case 'e':
			doerror = 1;
			break;
		case 'E':
			doerror2 = 1;
			break;
		default:
			fprintf(stderr, usage, version, argv[0]);
			exit(1);
		}

	argc -= optind;

	if ((argc != 2 && argc != 0) ||
	    (!doretrieve && data == NULL)) {
		fprintf(stderr, usage, version, argv[0]);
		exit(1);
	}

	argv += optind;
    
	if (argc == 2) {
		srch = get_handler(argv[0]);
		if (srch == NULL) {
			fprintf(stderr, "Unknown data type of %s\n", argv[0]);
			exit (1);
		}
		if (!doretrieve) {
			dsth = get_handler(argv[1]);
			if (dsth == NULL) {
				fprintf(stderr, "Unknown data type of %s\n",
					argv[1]);
				exit (1);
			}
		}
		fin = fopen(argv[0], "rb");
		if (fin == NULL) {
			fprintf(stderr, "Can't open input file '%s': ",
				argv[0]);
			perror("fopen");
			exit(1);
		}
		fout = fopen(argv[1], "wb");
		if (fout == NULL) {
			fprintf(stderr, "Can't open output file '%s': ",
				argv[1]);
			perror("fopen");
			exit(1);
		}
	} else {
		fin = stdin;
		fout = stdout;

		srch = dsth = get_handler(".ppm");
	}

	/* Initialize Golay-Tables for 12->23 bit error correction */
	if (doerror || doerror2) {
		fprintf(stderr, "Initalize encoding/decoding tables\n");
		init_golay();
	}

	if (doerror)
		flags |= STEG_ERROR;
    
	fprintf(stderr, "Reading %s....\n", argv[0]);
	image = srch->read(fin);
	fprintf(stderr, "Extracting usable bits ...\n");
	if (doretrieve)
		/* Wen extracting get the bitmap from the source handler */
		srch->get_bitmap(&bitmap, image, STEG_RETRIEVE);
	else {
		/* Initialize destination data handler */
		dsth->init(param);
		/* When embedding the destination format determines the bits */
		dsth->get_bitmap(&bitmap, image, 0);
	}

	/* Initialize random data stream */
	arc4_initkey(&as, key, strlen(key));
	tas = as;

	iterator_init(&iter, &bitmap, &as); 

	if (!doretrieve) {

		if (mark)
			flags |= STEG_MARK;
    
		if (useforbidden)
			flags |= STEG_FORBID;

		/* Encode the data for us */
		mmap_file(data, &data, &datalen);
		enclen = datalen;
		encdata = encode_data(data, &enclen, &tas, flags);
		if (flags & STEG_ERROR)
			fprintf(stderr, "Encoded data with ECC: %d\n", enclen);
		else
			fprintf(stderr, "Encoded data: %d\n", datalen);

		munmap_file(data, datalen);

		j = steg_find(&bitmap, &iter, &as, siter, siterstart,
			      encdata, enclen, flags);

		cumres = steg_embed(&bitmap, &iter, &as, encdata, enclen, j, flags | STEG_EMBED);

		free(encdata);

		if (key2 && data2) {
			char derivekey[128];
			struct arc4_stream tas;
			int i;

			if (doerror2)
				flags |= STEG_ERROR;
			else
				flags &= ~STEG_ERROR;

			encdata = NULL;
			for (j = -1, i = 0; i <= derive && j < 0; i++) {
#ifdef HAVE_SNPRINTF
				snprintf(derivekey, 128, "%s%d", key2, i);
#else /* YOU SUCK */
				sprintf(derivekey, "%s%d", key2, i);
#endif /* HAVE_SNPRINTF */
				if (i == 0)
					derivekey[strlen(key2)] = '\0';
				arc4_initkey(&as, derivekey, strlen(derivekey));
				iterator_init(&iter, &bitmap, &as); 

				tas = as;
				titer = iter;

				if (encdata)
					free (encdata);
				/* Map the file and encode its data */
				mmap_file(data2, &data, &datalen);
				enclen = datalen;
				encdata = encode_data(data, &enclen, &tas, flags);
				if (flags & STEG_ERROR)
					fprintf(stderr, "Encoded data with ECC: %d\n", enclen);
				else
					fprintf(stderr, "Encoded data: %d\n", datalen);
				munmap_file(data, datalen);
    
				fprintf(stderr, "2nd Embedding key: %s, %x\n",
					derivekey, arc4_getword(&tas));


				j = steg_find(&bitmap, &titer, &as, siter2, siterstart2,
					      encdata, enclen, flags);
			}
      
			if (j < 0) {
				fprintf(stderr, "Failed to find embedding.\n");
				exit (1);
			}

			tmpres = steg_embed(&bitmap, &iter, &as, encdata, enclen, j, flags | STEG_EMBED);
			cumres.changed += tmpres.changed;
			cumres.bias += tmpres.bias;
		}

		fprintf(stderr, "Total bits changed: %d (change %d + bias %d)\n",
			cumres.changed + cumres.bias,
			cumres.changed, cumres.bias);
		fprintf(stderr, "Storing bitmap into data...\n");
		dsth->put_bitmap (image, &bitmap, flags);

#ifdef FOURIER
		if (dofourier)
			fft_image(image->x, image->y, image->depth,
				  image->img);
#endif /* FOURIER */

		fprintf(stderr, "Writing %s....\n", argv[1]);
		dsth->write(fout, image);
	} else {
		encdata = steg_retrieve(&datalen, &bitmap, &iter, &as, flags);

		data = decode_data(encdata, &datalen, &tas, flags);
		free(encdata);

		fwrite(data, datalen, sizeof(u_char), fout);
		free(data);
	}

	free(bitmap.bitmap);
	free(bitmap.locked);

	free_pnm(image);

	return 0;
}


