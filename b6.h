/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2013 Roel Kluin 2013, GPL V3
 */

#ifndef RK_B6_H
#define RK_B6_H
#include <stdint.h>
#include <unistd.h>
#include "bits/bitasm.h"

typedef enum { alt_case = 0x20,	uc = 0x41,	lc = 0x61	} na_case;
typedef enum { alt_ribose = 0x1,deoxy = 0x11,	oxy = 0x10	} na_ribose;
typedef enum { alt_code = 0x40,	atob2 = 0x75,	b2toa = 0x35	} na_conversion;

/* convert ascii uc/lc DNA/RNA to 2bit format (on bits 2 and 3 -> 0x6) or back to
 * ascii the conversion is reversible, even for invalid characters
 *	 twobit		+- deoxy -+	+-- oxy -+	[na_ribose]
 * hex	 binary		uc	lc	uc	lc	[na_case]
 * 0x00	00000000	A	a	A	a
 * 0x02	00000010	C	c	C	c
 * 0x04	00000100	T	t	U	u
 * 0x06	00000110	G	G	G	g
 *	 atob2   <--->   b2toa [na_conversion]
 */
inline unsigned b6 (na_case cs, na_ribose r, na_conversion e, unsigned c)
{
	return c ^ cs ^ (-((c | 0x31) == e) & r);
}

/* With the conversion above only the na_* enums-specified characters are converted
 * to 2bits, left shifted by one, with all other bits zeroed. With the function
 * below one can test whether the ascii character prior to conversion was,
 * dependent on the enums specified, a valid convertable character.
 */
inline unsigned isb6 (unsigned b6)
{
	return (b6 | 0x6) == 0x6;
}

/* A similar, not entirely reversible conversion of ascii to 6b. It ignores case
 * and works for both DNA and RNA, but all characters in the range [@-GP-W`-gp-w]
 * produce values that can be interpreted as b6, including 'R', which occurs on
 * hg19, chr3.
 */
inline unsigned qb6 (unsigned c)
{
	return c ^ ((c & 0x31) | 0x40);
}

/* The macros and functions below are a similar conversion, but for uint64_t
 */
typedef enum { x32alt_case = 0x2020202020202020, x32uc = 0x4141414141414141,
	x32lc = 0x6161616161616161 } x32na_case;
typedef enum { x32alt_code = 0x4040404040404040, a_to_x32b2 = 0x7575757575757575,
	x32b2_to_a = 0x3535353535353535 } x32na_conversion;

#define __x32b2_convert(src, conv, r, tmp) ({			\
	tmp = conv ^ (src | 0x3131313131313131);		\
	tmp |= (tmp & 0xf0f0f0f0f0f0f0f0) >> 4;			\
	tmp |= (tmp & 0x0c0c0c0c0c0c0c0c) >> 2;			\
	tmp |= (tmp & 0x0202020202020202) >> 1;			\
	tmp = (tmp & 0x0101010101010101) ^ 0x0101010101010101;	\
	src ^ ((-(r == deoxy) & tmp) | (tmp << 4));		\
})

/* result 2bit order: 0-8-16-24 - 1-9-17-25 - 2-10-18-26 ... 7-15-23-31
 * this is inconvenient, but requires a lot less shifting.
 * after call, first check (*s | s[1] | s[2] | s[3])l if set there were Ns or alike
 */
uint64_t atox32b2 (const x32na_case cs, const na_ribose r, uint64_t s[4])
{
	uint64_t q, ret;
	q = cs ^ __x32b2_convert(*s, a_to_x32b2, r, q);
	*s = q & ~0x0606060606060606;
	ret = q >> 1;

	q = cs ^ __x32b2_convert(s[1], a_to_x32b2, r, q);
	s[1] = q & ~0x0606060606060606;
	ret |= q << 1;

	q = cs ^ __x32b2_convert(s[2], a_to_x32b2, r, q);
	s[2] = q & ~0x0606060606060606;
	ret |= q << 3;

	q = cs ^ __x32b2_convert(s[3], a_to_x32b2, r, q);
	s[3] = q & ~0x0606060606060606;
	return ret | q << 5;
}

/* s[0] should contain x322bit, this function sets s[0..3]
 */
void x32b2toa (const x32na_case cs, const na_ribose r, uint64_t s[4])
{
	uint64_t t, q, orig = *s;
	t = (orig & 0x0303030303030303) << 1;
	*s = cs ^ __x32b2_convert(t, x32b2_to_a, r, q);

	t = (orig >> 1) & 0x0606060606060606;
	s[1] = cs ^ __x32b2_convert(t, x32b2_to_a, r, q);

	t = (orig >> 3) & 0x0606060606060606;
	s[2] = cs ^ __x32b2_convert(t, x32b2_to_a, r, q);

	t = (orig >> 5) & 0x0606060606060606;
	s[3] = cs ^ __x32b2_convert(t, x32b2_to_a, r, q);
}

/* caller should have called  `c = b6(*c, *oxy, atob2, c)' and tested `isb6(c)'
 * the asmrol is needed due to the somewhat odd 2bit order.
 */
inline uint64_t x32b2_add_b2(uint64_t x32b2, unsigned c) {
	x32b2 = asm_rol(x32b2, 8);
	unsigned t = x32b2 & 0xff;
	return (x32b2 ^ t) | ((t >> 2) & 0x3f) | (c << 1);
}

#endif
