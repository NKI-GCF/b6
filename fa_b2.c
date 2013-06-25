/* Copyright (C) 2013 Roel Kluin GPL V3
 */

#if ZLIB_VER_MAJOR == 1 && ZLIB_VER_MINOR == 2 && ZLIB_VER_REVISION < 6

#error "Wrong zlib version"

#endif //ZLIB version 1.2.6 / 1.2.7


#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include "b6.h"

# define FA_BUFSIZE	(1 << 29)
# define B2_BUFSIZE	(FA_BUFSIZE >> 1)

#define B2_START	2u
#define B2_2BIT_INIT	4u
#define B2_2BIT0	7u
#define B2_2BIT1	1u
#define B2_2BIT2	3u
#define B2_2BIT3	5u
#ifndef B2_2BIT_DEBUG
# define B2_2BIT_DEBUG 0 //1024
#endif
#define B2_CHAR_INCOMPLETE	0x80

#define B2_GZOUT	(1 << 31)
#define SHFT_TOO_BIG	0xffffffff
// get bit for alpha char, regardless of case
#define amopt(r)		(1 << (r & 0x1f))
#define _setmopt(m, r)		m = (m | amopt(r))
#define _unsetmopt(m, r)	m = (m & ~amopt(r))

#define HERE fprintf(stderr, "%s:%u\n",__func__, __LINE__);

typedef struct fab2_t {
	gzFile gi; /* access to in gzip */
	char *start; /* first char in buffer, const for realloc */
	FILE *fin, *fout;
	void *vi, *vo;
	uint32_t blen; /* of buffer start */
	uint32_t mode;
	int (*write)(struct fab2_t*, int len);
} fab2_t;

# define _complete (gz->have + b - sq->start > sq->blen ? gzgetc(gz) : -1)

# define r_gzgetc(gz, ret, state) ({						\
	((gz)->have ? ((gz)->have--, (gz)->pos++, *((gz)->next)++) : (ret));	\
})

# define __next_b6(c, gz, state) ({					\
	do {								\
		c = r_gzgetc(gz, _complete, state);			\
	} while ((c != -1) && isspace(c));				\
	c = b6(uc, deoxy, atob2, c);					\
	isb6(c);							\
})

# define __store_completion(b, c, shft)					\
	if (shft) {							\
		*b++ |= shft << 6; /* store completion in high bits */	\
		*b++ = '\0';						\
		*b++ = c | B2_CHAR_INCOMPLETE;				\
	} else {							\
		*b++ = '\0';						\
		*b++ = c;						\
	}


static void b2seq_destroy(fab2_t *sq)
{
	int c;
	if (gzclose_r(sq->vi) != Z_OK)
		fprintf(stderr, "gzclose in: %s\n", gzerror(sq->vi, &c));

	if ((sq->mode & B2_GZOUT) && gzclose_w(sq->vo) != Z_OK)
		fprintf(stderr, "gzclose out: %s\n", gzerror(sq->vo, &c));

	if (sq)	free(sq->start);
}

static int b2gz_write(fab2_t *sq, int len)
{
	if ((len = gzwrite(sq->vo, sq->start, len)) < 0) {
		fprintf(stderr, "%s\n", gzerror(sq->vo, &len));
		return -3;
	}
	if (B2_2BIT_DEBUG)
		fprintf(stderr, "%u bytes written\n", len);

	return gzeof(sq->vo) ? -1 : len;
}

static int b2_write(fab2_t *sq, int len)
{
	if ((len = write(fileno(sq->fout), sq->start, len)) < 0) {
		fputs("error while writing\n", stderr);
		return -3;
	}
	if (B2_2BIT_DEBUG)
		fprintf(stderr, "%u bytes written\n", len);

	return ferror(sq->fout) ? -3 : len;
}


static char* fab2_init(fab2_t *sq)
{
	size_t sz = sq->mode & amopt('b') ? FA_BUFSIZE : B2_BUFSIZE;
	sq->vi = gzdopen(fileno(sq->fin), "rb");
	if (sq->vi == NULL) {
		fprintf(stderr, "%s(): can't gzopen in\n", __func__);
		return NULL;
	}
	if (gzbuffer(sq->vi, sz) == -1) {
		fprintf(stderr, "Error with gzbuffer(gzin, %lu)\n", sz);
		return NULL;
	}

	sz = sq->mode & amopt('b') ? B2_BUFSIZE : FA_BUFSIZE;
	if (sq->mode & B2_GZOUT) {
		sq->vo = gzdopen(fileno(sq->fout), "wb");
		if (sq->vo == NULL) {
			fprintf(stderr, "%s(): can't gzopen out\n", __func__);
			return NULL;
		}
		if (gzbuffer(sq->vo, sz) == -1) {
			fprintf(stderr, "Error with gzbuffer(gzout, %lu)\n", sz);
			return NULL;
		}
		sq->write = b2gz_write;
	} else {
		sq->write = b2_write;
	}
	/* according to zlib.h: the default buffer size is 8192 bytes. A
	larger buffer size of, for example, 64K or 128K bytes will noticeably
	increase the speed of decompression (reading). This uses 500Mb */
	char* b = malloc(B2_BUFSIZE);
	if (b != NULL) {
		sq->start = b;
		sq->blen = B2_BUFSIZE - 1;
	}
	return b;
}
/*
static inline uint64_t log2(uint64_t v)
{
	uint64_t r, shift;
	r = (v > 0xFFFFFFFF) << 5; v >>= r;
	shift = (v > 0xFFFF) << 4; v >>= shift; r |= shift;
	shift = (v > 0xFF) << 3; v >>= shift; r |= shift;
	shift = (v > 0xF) << 2; v >>= shift; r |= shift;
	shift = (v > 0x3) << 1; v >>= shift; r |= shift;
	return r | (v >> 1);
}

static unsigned fa_calc_entropy(const char* nts, unsigned ct, uint64_t* counts, unsigned asize)
{
	unsigned size = 0, entropy = 0;
	memset(counts, 0, sizeof(uint64_t) * asize);
	while (ct--) {
		unsigned c = b6(*nts);
		if (isb6(c)) { // N's are not counted.
			counts[c >> 1]++;
			size++;
		}
		++nts;
	}
	for (ct = asize; ct; --ct) {
		if (counts[ct] != 0) { // todo: size power of two log2

			// factor must be gt size * size to be able to distinguish
			// entropy in nt_entr[i] == (size-1) from nt_entr[i] == size
			// but maybe distinction up to 63 / 64 is enough
			// (64 times A doesn't occurr very often). => size * 64
			entropy += -counts[ct] * factor / size * log2(counts[ct] / size);
		}
	}
	return entropy;
}
*/

#define err_goto(ret, value, label) do {	\
		(ret) = (value);		\
		fprintf(stderr, "Error at %s:%u\n", __FILE__, __LINE__); \
		fflush(NULL);			\
		goto label;			\
	} while(0);

/*
 * This function parses fasta and produces a *series* of:
 *
 * >({seqname&comment}\n({2bit}|'\0\0'|'\0{char}'{stretch})*'\0>')*
 * {seqname&comment}\n({2bit}|'\0\0'|'\0{char}'{stretch})*'\0\n'
 *
 * where {seqname} and {comment} as well as {2bit} are Nul-less strings
 * {char} is a single character that was observed uint32_t {stretch} times
 * in the sequence (e.g. N's).
 *
 * In case of four A's (no 2b bits set) a '\0\0' is inserted. A sequence
 * ends with '\0>' if more sequences will follow, otherwise with '\0\n'
 *
 * if a 2bit was incomplete, the highest bit of the char after the Nul is
 * set. The number of nucleotides present is stored in the highest 2 bits
 * of the incomplete 2bit.
 *
 * a 2bit, ending with highest 2 bits unset followed by a Nul and a non-Nul
 * char with highest bit set, cannot occur.
 *
 * Return value:
    > 0	buffer ready
   -1   end-of-file
   -3   gzip error
   -4   truncated file
   -5   allocation error
   -6   wrong file format
 */
static int fa_b2(fab2_t *sq)
{
	char* b = sq->start == NULL ? fab2_init(sq) : sq->start;
	if (b == NULL)
		 return -5;

	gzFile gz = sq->vi;
	uint64_t state = B2_START;
	int c;
	*b++ = '>' | B2_CHAR_INCOMPLETE;
	/* skip to first */
	do {
		c = gzgetc(gz);
	} while ((c & 0x7f) != '>' && c != '@' && c != -1);
	if (c == -1)
		return -4;
	if (c != '>') {
		fprintf(stderr, "File format is %s\n", c == '@' ? "fastq" : "already 2bit");
		return -6;
	}

	do {
		switch (state) {

			/* header, ends at '\n', comment (if any) starts with '\0' */
			/* `c > 8' to exclude invalid space chars */
			char* q;
case B2_START:		q = b;
			//fprintf(stderr, "Start\n"); fflush(NULL);
			while (((c = r_gzgetc(gz, -1, state)) != '\n') && (c > 8))
				*b++ = c;
			if (c <= 8) {
				fprintf(stderr, "break in name\n"); fflush(NULL);
				if (c == -1) break;
				return -7;
			}
			*b = '\0';
			fprintf(stderr, "%s\n", q); fflush(NULL);
			*b++ = '\n';
			state = B2_2BIT0;

			unsigned i;
			uint64_t shft;
			*b = '\0';
case B2_2BIT0: case B2_2BIT1: case B2_2BIT2: case B2_2BIT3:
			shft = state;
			i = 0;

			char b2 = *b;
			while (__next_b6(c, gz, state)) {
				b2 |= c;
				if (shft == B2_2BIT0) {
					b2 <<= 2;
					shft = 1;
				} else if (shft == B2_2BIT1) {
					b2 <<= 2;
					shft += 2;
				} else if (shft == B2_2BIT2) {
					asm ("rolb $2, %0" : "=r" (b2) : "0" (b2));
					shft += 2;
				} else /*if (shft == B2_2BIT3)*/ {
					/* last rotate may not be necessary
					   if refasta is compliant */
					asm ("rorb $1, %0" : "=r" (b2) : "0" (b2));
					shft += 2;
					*b++ = b2;
					/* Nul is reserved, two Nuls for 4xA */
					if (b2 == '\0') {
						*b++ = '\0';
#if 0
						/* TODO: nice place to look for
						odd sequences, e.g. when 4 x A's
						occur more or less frequent than
						expected by chance, this could
						indicate repeats were inbetween.
						e.g. */

						if (*buffer - b > estimate) {
							/* do checks from *buffer
							to b */
						}
						*buffer = b;
#endif
					}
					b2 = '\0';
				}
				if (B2_2BIT_DEBUG && (++i % B2_2BIT_DEBUG == 0)) {
					c = b6(uc, deoxy, atob2, -1);
					break;
				}
			}
			c = b6(uc, deoxy, b2toa, c);
			//fprintf(stderr, "2bit end, %u, %d\n", shft, gz->have); fflush(NULL);
			if (c == -1) { /* i.e. -1 */
				*b = b2;
				state = shft;
				break;
			}
			//fprintf(stderr, "b2:%x, shft%lu\n", b2, shft);
			asm ("rorb $3, %0" : "=r" (b2) : "0" (b2));
			*b = b2;
			shft = ((shft + 1) & 7) >> 1;
			__store_completion(b, c, shft);

			if (c == '>') {
				//fprintf(stderr, "Done: state 0x%lx\n",state); fflush(NULL);
				state = B2_START;
				if (gz->have + b - sq->start > sq->blen)
					break;
				continue;
			}

			state = 8ul;
			int t;
default:
			if (c != 'N')
				fprintf(stderr, "strange character in stretch:%x %lu\n", c, state);
			//fprintf(stderr, "0x%lx %lu %c\n",state, shft, c); fflush(NULL);
			shft = state >> 3;
			do {
				t = r_gzgetc(gz, _complete, state);
				if (t == c) {
					++shft;
					continue;
				}
				if ((t == -1) || !isspace(t))
					break;
			} while (shft < SHFT_TOO_BIG);

			if (shft >= SHFT_TOO_BIG) {
				fprintf(stderr, "Too long stretch of non-nucleotides: %lu\n",shft);
				err_goto(c, -3, out);
			}
			//fprintf(stderr, "stretch was %u %x\n", shft, c);fflush(NULL);
			c = t;

			if (c != -1) {
				for (t = 0; t < 4; ++t, shft >>= 8)
					*b++ = shft & 0xff;
				*b = '\0';
				if (c != '>') {
					//fprintf(stderr, "X%lu %c\n", shft, c); fflush(NULL);
					gzungetc(c, gz);
					state = B2_2BIT0;
					continue;
				}
				++b;
				*b++ = '>';
				//fprintf(stderr, "Done: state 0x%lx\n",state); fflush(NULL);
				state = B2_START;
				if (gz->have + b - sq->start > sq->blen)
					break;
				continue;
			}
			state = shft << 3;
		}
		/* housekeeping, needed if gz->have == 0 */
		do {
			c = gzgetc(gz); /* function call: refills buffer */
		} while ((c != -1) && isspace(c));

		if (gzeof(gz) || c == -1) { /* finished! */
			fprintf(stderr, "finished\n"); fflush(NULL);
			c = '\n';
			if (state >= 8ul) { /* ends with a stretch */
				state >>= 3;
				int t;
				for (t = 0; t < 4; ++t, state >>= 8)
					*b++ = state & 0xff;
				*b++ = '\0';
				*b++ = '\n';
			} else if (state != B2_START) { /* ends with a 2bit */
				char b2 = *b;
				asm ("rorb $3, %0" : "=r" (b2) : "0" (b2));
				*b = b2;
				state = ((state + 1) & 7) >> 1;
				__store_completion(b, c, state);
			} else { /* ends inside name? error. */
				err_goto(c, -4, out);
			}
			c = -1;
			break;
		}
		/* have + at should be plenty: 2bit is smaller */
		size_t at = b - sq->start;
		if (gz->have + at > sq->blen || B2_2BIT_DEBUG) {
			//fprintf(stderr, "writing buffer...\n"); fflush(NULL);
			if (sq->write(sq, at) < 0) {
				fprintf(stderr, "0x%lx %x\n",state, c); fflush(NULL);
				err_goto(c, -3, out);
			}
			if (state != B2_START) {
				//fprintf(stderr, "<%lu %x\n", state, *b); fflush(NULL);
				*sq->start = *b;
			}
			b = sq->start;
		}
		gzungetc(c, gz);
	} while (1);
	size_t at = b - sq->start;
	if (at) {
		fprintf(stderr, "writing last buffer...\n");
		fflush(NULL);
		if (sq->write(sq, at) < 0)
			c = -3;
	}
out:
	return c;
}

/* Return value:
    > 0	buffer ready
   -1   end-of-file
   -3   gzip error
   -4   truncated file
   -5   allocation error
   -6   wrong file format
 */

static int b2_fa(fab2_t *sq)
{
	char* b = sq->start == NULL ? fab2_init(sq) : sq->start;
	if (b == NULL)
		 return -5;

	gzFile gz = sq->vi;
	uint64_t state = B2_START;
	int c, i = 0, nc = '\0';
	unsigned linelen = 60;

	c = gzgetc(gz);
	if (c == -1)
		return -4;
	if (c != ('>' | B2_CHAR_INCOMPLETE)) {
		fprintf(stderr, "File format is not 2bit\n");
		return -6;
	}

	do {
		switch (state) {
case B2_START:
			*b++ = '>';
			char* q = b;
			while ((c = gzgetc(gz)) != '\n') {
				if (c == -1) {
					err_goto(c, -4, out);
				}
				*b++ = c;
			}
			*b = '\0';
			fprintf(stderr, "%s\n", q);fflush(NULL);
			*b++ = c;
			i = 0;

			c = gzgetc(gz);
			if (c == -1)
				err_goto(c, -3, out);
			if (c == '\0') {
				c = gzgetc(gz);
				//fprintf(stderr, "xxx.c:%x, nc:%x, i:%u\n", c, nc, i);
				if (c != '\0') { /* last 2bit */
					if (c & B2_CHAR_INCOMPLETE) {
						if (c == -1)
							err_goto(c, -3, out);
						err_goto(c, -6, out);
					}
					nc = c;
					goto boo;
				}
			}

case B2_2BIT_INIT:	// we have to look ahead for an end character,
			// then 2bit may be incomplete
			nc = gzgetc(gz);
			if (nc == -1)
				err_goto(nc, -3, out)
			if (nc == '\0') {
				nc = gzgetc(gz);
				if (nc == -1)
					err_goto(nc, -3, out)
				if (nc != '\0') { /* last 2bit */
					if (nc & B2_CHAR_INCOMPLETE) {
						nc ^= B2_CHAR_INCOMPLETE;
						state = (((c & 0xc0) >> 5) - 1) & 7;
						//fprintf(stderr, "state:%lu, c:%x, nc:%x, i:%u\n", state, c, nc, i);
						if (state == B2_2BIT0) /* complete? */
							err_goto(c, -6, out);
						continue;
					}
					state = B2_2BIT0;
					//fprintf(stderr, "2bit end:state:%lx, c:%x\n", state,c);fflush(NULL);
					continue;
				}
			}
			state = B2_2BIT_INIT;
case B2_2BIT0:		*b++ = b6(uc, deoxy, b2toa, (c >> 5) & 6);
			if ((++i % linelen) == 0)
				*b++ = '\n';
case B2_2BIT3:		*b++ = b6(uc, deoxy, b2toa, (c >> 3) & 6);
			if ((++i % linelen) == 0)
				*b++ = '\n';
case B2_2BIT2:		*b++ = b6(uc, deoxy, b2toa, (c >> 1) & 6);
			if ((++i % linelen) == 0)
				*b++ = '\n';
case B2_2BIT1:		*b++ = b6(uc, deoxy, b2toa, (c << 1) & 6);
			if ((++i % linelen) == 0)
				*b++ = '\n';
boo:
			//fprintf(stderr, "...c:%x, nc:%x, i:%u\n", c, nc, i);
			c = nc;
			if (state == B2_2BIT_INIT)
				continue;
			//fprintf(stderr, "2bit state was %lx, %u, %c\n", state, i, c); fflush(NULL);

			size_t at = b - sq->start;
			/* FIXME: writing too often */
			//fprintf(stderr, "writing buffer, (%lu) %p %p <%p>...\n", at, b, sq->start, sq->vo);
			//fflush(NULL);
			//fprintf(stderr, "gzflushed...\n"); fflush(NULL);
			if ((nc = sq->write(sq, at)) < 0)
				err_goto(c, -3, out);

			//fprintf(stderr, "%u bytes written c:%x\n", nc, c); fflush(NULL);
			b = sq->start;
			//assert((i % linelen) == 0 || *b != '\n');

			if (c == '>') {
				if ((i % linelen) != 0)
					*b++ = '\n';
				//fprintf(stderr, "2bit state was %lx, %u, %c\n", state, i, c); fflush(NULL);
				state = B2_START;
				continue;
			}

			if (c == '\n' || c == 0x7f || c == 0xff) {
				fprintf(stderr, "finished, %u\n", i % linelen); fflush(NULL);
				c = -1;
				break;
			}

			unsigned t, stretch = 0u;
			//fprintf(stderr, "start stretch of %x\n", c); fflush(NULL);
			for (t = 0; t < 4; ++t) {
				nc = gzgetc(gz);
				if (nc == -1)
					err_goto(c, -3, out);
				//fprintf(stderr, "%x, %u\n", nc, nc << (8 * (3-i)));
				stretch |= nc << (t << 3);
			}
			if (c != 'N') {
				fprintf(stderr, "== stretch of %u x '%c' (%x) ==\n", stretch, c, c);
				fflush(NULL);
			}
			if (stretch == 0u)
				err_goto(c, -6, out);
			//assert((i % linelen) == 0 || *b != '\n');
			at = b - sq->start;
			do {
				if (stretch + (stretch / linelen) + at >= FA_BUFSIZE / 2 - 2) {
					t = (FA_BUFSIZE - (FA_BUFSIZE / linelen))/2 - 2 - at;
				} else {
					t = stretch;
				}
				//fprintf(stderr, "t:%u, at:%lu, BS:%u, stch:%u\n",
				//	t, at, FA_BUFSIZE, stretch);
				fflush(NULL);
				stretch -= t;
				while (t--) {
					*b++ = c;
					if ((++i % linelen) == 0) {//263884215
						//fprintf(stderr, "%u \n", t);fflush(NULL);
						*b++ = '\n';
					}
				}
				at = b - sq->start;
				if ((nc = sq->write(sq, at)) < 0)
					err_goto(c, -3, out);

				b = sq->start;
				at = 0;
			} while (stretch);
			//gzflush(sq->vo, Z_FINISH);/////

			c = gzgetc(gz);
			if (c == '\0') {
				nc = gzgetc(gz);
				if (nc == '\n' || nc == 0x7f || nc == -1) {
					//fprintf(stderr, "end of seq\n"); fflush(NULL);
					c = -1;
				} else if (nc == '\0') {
					state = B2_2BIT_INIT;
				} else if (nc == '>') {
					if ((i % linelen) != 0)
						*b++ = '\n';
					state = B2_START;
				} else {
					fprintf(stderr, "err:state:%lx, c:%x, nc:%x, i:%u\n", state, c, nc, i); fflush(NULL);
					err_goto(c, -6, out);
				}
			} else if (c != -1) {
				state = B2_2BIT_INIT;
			} else {
				err_goto(nc, -3, out)
			}
		}
		//fprintf(stderr, "loop:state:%lx, c:%x, nc:%x, i:%u\n", state, c, nc, i); fflush(NULL);
	} while (c != -1); /* end switch(state) */
	//fprintf(stderr, "writing buffer state:%lx c:%x, nc:%x, i:%u\n", state, c, nc, i);
	//fflush(NULL);
	if ((i % linelen) != 0)
		*b++ = '\n';
	size_t at = b - sq->start;
	/* FIXME: writing too often */
	if (sq->write(sq, at) < 0) {
		fprintf(stderr, "%x %lx\n", c, (uint64_t)(b - sq->start));
		//err_goto(c, -3, out);
	}
out:
	//fprintf(stderr, "out state:%lx c:%x, nc:%x, i:%u\n", state, c, nc, i);
	//fflush(NULL);
	return c;
}

FILE* getfp(const char* fname, unsigned* mode)
{
	/* order IN/B2_GZOUT is important */
	if (fname == NULL) {
		if (*mode & B2_GZOUT) {
			*mode &= ~B2_GZOUT;
			return stdout; // output to stdout is uncompressed
		}
		return stdin;
	}
	unsigned e = strlen(fname) - 3;
	if ((*mode & B2_GZOUT) && (fopen(fname, "r") != NULL) && (amopt('f') != 0)) {
		fprintf(stderr, "%s already exists\n", fname);
	} else if (e > 0) {
		// (byte1 == 0x1f) && (byte2 == 0x8b) // for gzipped (not together => endianness).
		// TODO : look at content rather than extension
		if (strstr(fname, ".gz") == fname + e)
			return fopen(fname, *mode & B2_GZOUT ? "w" : "r");

		if ((strstr(fname, ".fa") == fname + e) ||
				(strstr(fname, ".2b") == fname+e) ||
				((e > 3) && (strstr(fname, ".fasta") == fname + e - 3))) {
			if (*mode & B2_GZOUT) {
				*mode &= ~B2_GZOUT;
				return fopen(fname, "w");
			}
			return fopen(fname, "r");
		}
	}
	return NULL;
}

int main(int argc, char * const argv[])
{
	const char *in = NULL, *out = NULL;
	fab2_t sq = {0};
	int c;
	sq.mode = amopt('b');

	do {
		static struct option lopt[] = {
			{"fasta-to-b2", no_argument, NULL, 'b'},
			{"b2-to-fasta", no_argument, NULL, 'B'},
			{"force", no_argument, NULL, 'f'},
			{"in", required_argument, NULL, 'i'},
			{"out",	required_argument, NULL, 'o'},
			{NULL, 0, NULL, 0}
		};
		int opti = 0;
		c = getopt_long_only(argc, argv, "bBfi:o:", lopt, &opti);
		switch (c) {
case 'i':		in = optarg; break;
case 'o':		out = optarg; break;
case 'b': case 'f':	_setmopt(sq.mode, c); break;
case 'B':		_unsetmopt(sq.mode, c); break;
case -1 ... 0:		break;
case ':':		fprintf(stderr, "%s: -%c missing argument\n", argv[0], c);
case '?':		goto out;
default:		fprintf(stderr, "%s: -%c is invalid\n", argv[0], c);
			goto out;
		}
	} while (c != -1);

	for (;optind != argc; ++optind) {
		if (in == NULL) {
			in = argv[optind];
		} else if (out == NULL) {
			out = argv[optind];
		} else {
			fprintf(stderr, "%s: Unrecognized argument %s\n",
					argv[0], argv[optind]);
			goto out;
		}
	}

	sq.fin = getfp(in, &sq.mode);
	if (sq.fin == NULL) {
		fprintf(stderr, "error opening %s for reading\n", in);
		goto out;
	}

	sq.mode |= B2_GZOUT;
	sq.fout = getfp(out, &sq.mode);
	if (sq.fout == NULL) {
		fprintf(stderr, "error opening %s for writing\n", out);
		fclose(sq.fin);
		goto out;
	}

	if (sq.mode & amopt('b')) {
		fprintf(stderr, "conversion from fasta to 2bit\n");
		c = fa_b2(&sq);
	} else {
		fprintf(stderr, "conversion from 2bit to fasta\n");
		c = b2_fa(&sq);
	}
	if (c < -1)
		fprintf(stderr, "fa_b2() error: %d\n", c);
	if (sq.start != NULL)
		b2seq_destroy(&sq);

	fclose(sq.fin);
	fclose(sq.fout);
out:	return 0;
}

