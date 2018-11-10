/*
 * Outguess - a Universal Steganograpy Tool for
 *
 * Copyright (c) 1999-2001 Niels Provos <provos@citi.umich.edu>
 * Features
 * - preserves frequency count based statistics
 * - multiple data embedding
 * - PRNG driven selection of bits
 * - error correcting encoding
 * - modular architecture for different selection and embedding algorithms
 */

/*
 * Copyright (c) 1999-2001 Niels Provos <provos@citi.umich.edu>
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
#include <netinet/in.h>
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
#include "iterator.h"

#ifndef MAP_FAILED
/* Some Linux systems are missing this */
#define MAP_FAILED	(void *)-1
#endif /* MAP_FAILED */

static int steg_err_buf[CODEBITS];
static int steg_err_cnt;
static int steg_errors;
static int steg_encoded;

static int steg_offset[MAX_SEEK];
int steg_foil;
int steg_foilfail;

static int steg_count;
static int steg_mis;
static int steg_mod;
static int steg_data;

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
			WRITE_BIT(bitmap->locked, i, 0);
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
	int i = ITERATOR_CURRENT(iter);
	u_int8_t bit;
	u_int32_t val;
	u_char *pbits, *plocked;
	int nbits;

	pbits = bitmap->bitmap;
	plocked = bitmap->locked;
	nbits = bitmap->bits;

	while (i < nbits && bits) {
		if ((embed & STEG_ERROR) && !steg_encoded) {
			if (steg_err_cnt > 0)
				steg_adjust_errors(bitmap, embed);
			steg_encoded = CODEBITS;
			steg_errors = 0;
			steg_err_cnt = 0;
			memset(steg_err_buf, 0, sizeof(steg_err_buf));
		}
		steg_encoded--;

		bit = TEST_BIT(pbits, i) ? 1 : 0;
		val = bit ^ (data & 1);
		steg_count++;
		if (val == 1) {
			steg_mod += bitmap->detect[i];
			steg_mis++;
		}

		/* Check if we are allowed to change a bit here */
		if ((val == 1) && TEST_BIT(plocked, i)) {
			if (!(embed & STEG_ERROR) || (++steg_errors > 3))
				return 0;
			val = 2;
		}

		/* Store the bits we changed in error encoding mode */
		if ((embed & STEG_ERROR) && val == 1)
			steg_err_buf[steg_err_cnt++] = i;

		if (val != 2 && (embed & STEG_EMBED)) {
			WRITE_BIT(plocked, i, 1);
      			WRITE_BIT(pbits, i, data & 1);
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
		fprintf(stderr, "steg_embed: not enough bits in bitmap "
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

	while (ITERATOR_CURRENT(iter) < bitmap->bits && datalen > 0) {
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

	if (embed & STEG_EMBED) {
		fprintf(stderr, "Bits embedded: %d, "
			"changed: %d(%2.1f%%)[%2.1f%%], "
			"bias: %d, tot: %d, skip: %d\n",
			steg_count, steg_mis,
			(float) 100 * steg_mis/steg_count,
			(float) 100 * steg_mis/steg_data, /* normalized */
			steg_mod,
			ITERATOR_CURRENT(iter),
			ITERATOR_CURRENT(iter) - steg_count);
	}

	result.changed = steg_mis;
	result.bias = steg_mod;

	return result;
}

u_int32_t
steg_retrbyte(bitmap *bitmap, int bits, iterator *iter)
{
	u_int32_t i = ITERATOR_CURRENT(iter);
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

			/* 
			 * Only count bias, if we do not modifiy many
			 * extra bits for statistical foiling.
			 */
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
				fprintf(stderr, "%5u: %5u(%3.1f%%)[%3.1f%%], bias %5d(%1.2f), saved: % 5d, total: %5.2f%%\n",
					j, result.changed, 
					(float) 100 * steg_mis / steg_count,
					(float) 100 * steg_mis / steg_data,
					result.bias, 
					(float)result.bias / steg_mis,
					(half - result.changed) / 8,
					(float) 100 * steg_mis / bitmap->bits);
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

int
do_embed(bitmap *bitmap, u_char *filename, u_char *key, u_int klen,
	 config *cfg, stegres *result)
{
	iterator iter;
	struct arc4_stream as, tas;
	u_char *encdata, *data;
	u_int datalen, enclen;
	size_t correctlen;
	int j;

	/* Initialize random data stream */
	arc4_initkey(&as,  "Encryption", key, klen);
	tas = as;
		
	iterator_init(&iter, bitmap, key, klen); 

	/* Encode the data for us */
	mmap_file(filename, &data, &datalen);
	steg_data = datalen * 8;
	enclen = datalen;
	encdata = encode_data(data, &enclen, &tas, cfg->flags);
	if (cfg->flags & STEG_ERROR) {
		fprintf(stderr, "Encoded '%s' with ECC: %d bits, %d bytes\n",
			filename, enclen * 8, enclen);
		correctlen = enclen / 2 * 8;
	} else {
		fprintf(stderr, "Encoded '%s': %d bits, %d bytes\n",
			filename, enclen * 8, enclen);
		correctlen = enclen * 8;
	}
	if (bitmap->maxcorrect && correctlen > bitmap->maxcorrect) {
		fprintf(stderr, "steg_embed: "
			"message larger than correctable size %d > %d\n",
			correctlen, bitmap->maxcorrect);
		exit(1);
	}

	munmap_file(data, datalen);

	j = steg_find(bitmap, &iter, &as, cfg->siter, cfg->siterstart,
		      encdata, enclen, cfg->flags);
	if (j < 0) {
		fprintf(stderr, "Failed to find embedding.\n");
		goto out;
	}

	*result = steg_embed(bitmap, &iter, &as, encdata, enclen, j,
			    cfg->flags | STEG_EMBED);

 out:
	free(encdata);

	return (j);
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
	char version[] = "OutGuess 0.2 Universal Stego (c) 1999-2001 Niels Provos";
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
		"\t-F[+-]       turns statistical steganalysis foiling on/off.\n"
		"\t             The default is on.\n"
#ifdef FOURIER
		"\t-f           fourier transform modified image\n"
#endif /* FOURIER */
		;

	char *progname;
	FILE *fin = stdin, *fout = stdout;
	image *image;
	handler *srch = NULL, *dsth = NULL;
	char *param = NULL;
	unsigned char *encdata;
	bitmap bitmap;	/* Extracted bits that we may modify */
	iterator iter;
	int j, ch, derive = 0;
	stegres cumres, tmpres;
	config cfg1, cfg2;
	u_char *data = NULL, *data2 = NULL;
	int datalen;
	char *key = "Default key", *key2 = NULL;
	struct arc4_stream as, tas;
	char mark = 0, doretrieve = 0;
	char doerror = 0, doerror2 = 0;
	char *cp;
	int extractonly = 0, foil = 1;
#ifdef FOURIER
	char dofourier = 0;
#endif /* FOURIER */

	progname = argv[0];
	steg_stat = 0;

	memset(&cfg1, 0, sizeof(cfg1));
	memset(&cfg2, 0, sizeof(cfg2));

        if (strchr(argv[0], '/'))
                cp = strrchr(argv[0], '/') + 1;
        else
                cp = argv[0];
	if (!strcmp("extract", cp)) {
		extractonly = 1;
		doretrieve = 1;
		argv++;
		argc--;
		goto aftergetop;
	}

	/* read command line arguments */
	while ((ch = getopt(argc, argv, "eErmftp:s:S:i:I:k:d:D:K:x:F:")) != -1)
		switch((char)ch) {
		case 'F':
			if (optarg[0] == '-')
				foil = 0;
			break;
		case 'k':
			key = optarg;
			break;
		case 'K':
			key2 = optarg;
			break;
		case 'p':
			param = optarg;
			break;
		case 'x':
			derive = atoi(optarg);
			break;
		case 'i':
			cfg1.siter = atoi(optarg);
			break;
		case 'I':
			cfg2.siter = atoi(optarg);
			break;
		case 'r':
			doretrieve = 1;
			break;
		case 't':
			steg_stat++;
			break;
		case 's':
			cfg1.siterstart = atoi(optarg);
			break;
		case 'S':
			cfg2.siterstart = atoi(optarg);
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
	argv += optind;

 aftergetop:    
	if ((argc != 2 && argc != 0) ||
	    (extractonly && argc != 2) ||
	    (!doretrieve && !extractonly && data == NULL)) {
		fprintf(stderr, usage, version, progname);
		exit(1);
	}

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

	fprintf(stderr, "Reading %s....\n", argv[0]);
	image = srch->read(fin);
	
	if (extractonly) {
		int bits;
		/* Wen extracting get the bitmap from the source handler */
		srch->get_bitmap(&bitmap, image, STEG_RETRIEVE);

		fprintf(stderr, "Writing %d bits\n", bitmap.bits);
		bits = htonl(bitmap.bits);
		fwrite(&bits, 1, sizeof(int), fout);
		fwrite(bitmap.bitmap, bitmap.bytes, sizeof(char), fout);
		exit (1);
	} else if (doretrieve)
		/* Wen extracting get the bitmap from the source handler */
		srch->get_bitmap(&bitmap, image, STEG_RETRIEVE);
	else {
		/* Initialize destination data handler */
		dsth->init(param);
		/* When embedding the destination format determines the bits */
		dsth->get_bitmap(&bitmap, image, 0);
	}
	fprintf(stderr, "Extracting usable bits:   %d bits\n", bitmap.bits);

	if (doerror)
		cfg1.flags |= STEG_ERROR;
    
	if (!doretrieve) {
		if (mark)
			cfg1.flags |= STEG_MARK;
		if (foil) {
			dsth->preserve(&bitmap, -1);
			if (bitmap.maxcorrect)
				fprintf(stderr,
					"Correctable message size: %d bits, %0.2f%%\n",
					bitmap.maxcorrect,
					(float)100*bitmap.maxcorrect/bitmap.bits);
		}

		do_embed(&bitmap, data, key, strlen(key), &cfg1, &cumres);

		if (key2 && data2) {
			char derivekey[128];
			int i;

			/* Flags from first configuration are being copied */
			cfg2.flags = cfg1.flags;
			if (doerror2)
				cfg2.flags |= STEG_ERROR;
			else
				cfg2.flags &= ~STEG_ERROR;

			for (j = -1, i = 0; i <= derive && j < 0; i++) {
#ifdef HAVE_SNPRINTF
				snprintf(derivekey, 128, "%s%d", key2, i);
#else /* YOU SUCK */
				sprintf(derivekey, "%s%d", key2, i);
#endif /* HAVE_SNPRINTF */
				if (i == 0)
					derivekey[strlen(key2)] = '\0';

				j = do_embed(&bitmap, data2,
					     derivekey, strlen(derivekey),
					     &cfg2, &tmpres);
			}
      
			if (j < 0) {
				fprintf(stderr, "Failed to find embedding.\n");
				exit (1);
			}

			cumres.changed += tmpres.changed;
			cumres.bias += tmpres.bias;
		}

		if (foil) {
			int i, count;
			double mean, dev, sq;
			int n;
			u_char cbit;
			u_char *pbits = bitmap.bitmap;
			u_char *data = bitmap.data;
			u_char *plocked = bitmap.locked;
    
			memset(steg_offset, 0, sizeof(steg_offset));
			steg_foil = steg_foilfail = 0;

			for (i = 0; i < bitmap.bits; i++) {
				if (!TEST_BIT(plocked, i))
					continue;

				cbit = TEST_BIT(pbits, i) ? 1 : 0;

				if (cbit == (data[i] & 0x01))
					continue;

				n = bitmap.preserve(&bitmap, i);
				if (n > 0) {
					/* Actual modificaton */
					n = abs(n - i);
					if (n > MAX_SEEK)
						n = MAX_SEEK;

					steg_offset[n - 1]++;
				}
			}

			/* Indicates that we are done with the image */
			bitmap.preserve(&bitmap, bitmap.bits);

			/* Calculate statistics */
			count = 0;
			mean = 0;
			for (i = 0; i < MAX_SEEK; i++) {
				count += steg_offset[i];
				mean += steg_offset[i] * (i + 1);
			}
			mean /= count;

			dev = 0;
			for (i = 0; i < MAX_SEEK; i++) {
				sq = (i + 1 - mean) * (i + 1 - mean);
				dev += steg_offset[i] * sq;
			}
			
			fprintf(stderr, "Foiling statistics: "
				"corrections: %d, failed: %d, "
				"offset: %f +- %f\n",
				steg_foil, steg_foilfail,
				mean, sqrt(dev / (count - 1)));
		}

		fprintf(stderr, "Total bits changed: %d (change %d + bias %d)\n",
			cumres.changed + cumres.bias,
			cumres.changed, cumres.bias);
		fprintf(stderr, "Storing bitmap into data...\n");
		dsth->put_bitmap (image, &bitmap, cfg1.flags);

#ifdef FOURIER
		if (dofourier)
			fft_image(image->x, image->y, image->depth,
				  image->img);
#endif /* FOURIER */

		fprintf(stderr, "Writing %s....\n", argv[1]);
		dsth->write(fout, image);
	} else {
		/* Initialize random data stream */
		arc4_initkey(&as,  "Encryption", key, strlen(key));
		tas = as;
	  
		iterator_init(&iter, &bitmap, key, strlen(key)); 

		encdata = steg_retrieve(&datalen, &bitmap, &iter, &as,
					cfg1.flags);

		data = decode_data(encdata, &datalen, &tas, cfg1.flags);
		free(encdata);

		fwrite(data, datalen, sizeof(u_char), fout);
		free(data);
	}

	free(bitmap.bitmap);
	free(bitmap.locked);

	free_pnm(image);

	return 0;
}


