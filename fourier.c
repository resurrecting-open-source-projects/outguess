/*
 * Copyright (C) 1999, 2000 Niels Provos <provos@citi.umich.edu>
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

#include <stdio.h>
#include "fourier.h"

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
