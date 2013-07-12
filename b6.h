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
#  define RK_B6_H
#  define BITS_PER_LONG 64

#  include <stdint.h>
#  include <unistd.h>
#  include "bits/bitasm.h"
#  include "bits/revbin.h"
#  include "bits/bitcount.h"

typedef enum { alt_case = 0x20, uc = 0x41, lc = 0x61 } na_case;
typedef enum { alt_ribose = 0x1, deoxy = 0x11, oxy = 0x10 } na_ribose;
typedef enum { alt_code = 0x40, atob2 = 0x75, b2toa = 0x35 } na_conversion;

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
inline unsigned
b6(na_case cs, na_ribose r, na_conversion e, unsigned c)
{
	return c ^ cs ^ (-((c | 0x31) == e) & r);
}

/* With the conversion above only the na_* enums-specified characters are converted
 * to 2bits, left shifted by one, with all other bits zeroed. With the function
 * below one can test whether the ascii character prior to conversion was,
 * dependent on the enums specified, a valid convertable character.
 */
inline unsigned
isb6(unsigned b6)
{
	return (b6 | 0x6) == 0x6;
}

/* A similar, not entirely reversible conversion of ascii to b6. It ignores case
 * and works for both DNA and RNA, but all characters in the range [@-GP-W`-gp-w]
 * produce values interpreted as b6, including 'R', which occurs on hg19, chr3.
 */
inline unsigned
qb6(unsigned c)
{
	return c ^ ((c & 0x31) | 0x40);
}

/* The macros and functions below are a similar conversion, for uint64_t
typedef enum { x32alt_case = 0x2020202020202020, x32uc = 0x4141414141414141,
	x32lc = 0x6161616161616161
} x32na_case;
typedef enum { x32alt_code = 0x4040404040404040, a_to_x32b2 =
		0x7575757575757575,
	x32b2_to_a = 0x3535353535353535
} x32na_conversion;*/

#  define __x32b2_convert(src, conv, r, tmp) ({			\
	tmp = conv ^ (src | 0x3131313131313131);		\
	tmp |= (tmp & 0xf0f0f0f0f0f0f0f0) >> 4;			\
	tmp |= (tmp & 0x0c0c0c0c0c0c0c0c) >> 2;			\
	tmp |= (tmp & 0x0202020202020202) >> 1;			\
	tmp = (tmp & 0x0101010101010101) ^ 0x0101010101010101;	\
	src ^ ((-(r == deoxy) & tmp) | (tmp << 4));		\
})

#  define __M_SWAP(x, t, m1, m2, shft) ({		\
	x ^= (t = x & m1);				\
	x ^= (t = asm_rol(t, shft));			\
	t ^= x & m2;					\
	x ^ t ^ asm_ror(t, shft);			\
})

// twobits foreach b8 end up next to one another. XXX: Without this reordering
// we may gain speed but that will complicate partial sequence matching
#define __order_x32b8(r, t) ({							\
	r = __M_SWAP(r, t, 0x00cc00cc00cc00cc, 0x3300330033003300, 10);		\
	r = __M_SWAP(r, t, 0x0000f0f00000f0f0, 0x0f0f00000f0f0000, 12);		\
	r = __M_SWAP(r, t, 0x00000000ff00ff00, 0x00ff00ff00000000, 24);		\
	__M_SWAP(r, t, 0x00000000ffff0000, 0x0000ffff00000000, 16);		\
})

#define __scramble_x32b8(r, t) ({						\
	r = __M_SWAP(r, t, 0x00000000ffff0000, 0x0000ffff00000000, 16);		\
	r = __M_SWAP(r, t, 0x00000000ff00ff00, 0x00ff00ff00000000, 24);		\
	r = __M_SWAP(r, t, 0x0000f0f00000f0f0, 0x0f0f00000f0f0000, 12);		\
	__M_SWAP(r, t, 0x00cc00cc00cc00cc, 0x3300330033003300, 10);		\
})

/* result 2bit order: 0-8-16-24 - 1-9-17-25 - 2-10-18-26 ... 7-15-23-31
 * inconvenient, but requires a lot less shifting. After call, check that
 * (s[0] | s[1] | s[2] | s[3]) is zero. if set there were Ns or strange characters.
 */
uint64_t
uo_atox32b2(const na_case cs, const na_ribose r, uint64_t s[4])
{
	uint64_t q, ret, a_to_x32b2 = atob2 * 0x0101010101010101;
	uint64_t x32cs = cs * 0x0101010101010101;

	q = x32cs ^ __x32b2_convert(*s, a_to_x32b2, r, q);
	*s = q & ~0x0606060606060606;
	ret = q >> 1;

	q = x32cs ^ __x32b2_convert(s[1], a_to_x32b2, r, q);
	s[1] = q & ~0x0606060606060606;
	ret |= q << 1;

	q = x32cs ^ __x32b2_convert(s[2], a_to_x32b2, r, q);
	s[2] = q & ~0x0606060606060606;
	ret |= q << 3;

	q = x32cs ^ __x32b2_convert(s[3], a_to_x32b2, r, q);
	s[3] = q & ~0x0606060606060606;
	ret |= q << 5;
	return ret; //__order_x32b8(ret, q);
}

/* s[0] should contain x32bit at call, sets s[0..3] to ascii.
 */
void
uo_x32b2toa(const na_case cs, const na_ribose r, uint64_t s[4])
{
	uint64_t t, q, orig = *s, x32b2_to_a = b2toa * 0x0101010101010101;
	//orig = __scramble_x32b8(orig, t);
	uint64_t x32cs = cs * 0x0101010101010101;

	t = (orig & 0x0303030303030303) << 1;
	*s = x32cs ^ __x32b2_convert(t, x32b2_to_a, r, q);

	t = (orig >> 1) & 0x0606060606060606;
	s[1] = x32cs ^ __x32b2_convert(t, x32b2_to_a, r, q);

	t = (orig >> 3) & 0x0606060606060606;
	s[2] = x32cs ^ __x32b2_convert(t, x32b2_to_a, r, q);

	t = (orig >> 5) & 0x0606060606060606;
	s[3] = x32cs ^ __x32b2_convert(t, x32b2_to_a, r, q);
}

 /* TODO: function that orders 2bit, a least for testing */
uint64_t
atox32b2(const na_case cs, const na_ribose r, uint64_t s[4])
{
	uint64_t  ret = 0ul;
	unsigned i, c, gotN = 0;
	for (i = 0; i != 32; ++i) {
		c = b6(cs, r, atob2, s[i >> 3] & 0xff);
		ret |= (uint64_t)((c & 6) >> 1) << (i << 1);
		s[i >> 3] >>= 8;
		gotN |= !isb6(c) << i;
	}
	s[0] = s[1] = s[2] = s[3] = gotN;
	return ret;
}

void
x32b2toa(const na_case cs, const na_ribose r, uint64_t s[4])
{
	uint64_t  orig = *s;
	unsigned i, c;
	s[0] = s[1] = s[2] = s[3] = 0;
	for (i = 0; i != 32; ++i) {
		c = b6(cs, r, b2toa, ((orig >> (i << 1)) << 1) & 0x6);
		s[i >> 3] |= (uint64_t)c << ((i & 7) << 3);
	}
}



/* caller should have called`c = b6(*c, *oxy, atob2, c)' and tested `isb6(c)'
 * the asmrol is needed due to the somewhat odd 2bit order.
 */
inline uint64_t
x32b2_add_b6(uint64_t x32b2, unsigned c)
{
	x32b2 = asm_rol(x32b2, 8);
	unsigned t = x32b2 & 0xff;

	return (x32b2 ^ t) | ((t >> 2) & 0x3f) | (c << 1);
}

/* add eight ascii characters, caller should check that *s is zero or Ns occured
 */
inline uint64_t
x32b2_add_8a(const na_case cs, const na_ribose r, uint64_t x32b2, uint64_t * s)
{
	uint64_t q;
	uint64_t x32cs = cs * 0x0101010101010101,
		 a_to_x32b2 = atob2 * 0x0101010101010101;

	q = x32cs ^ __x32b2_convert(*s, a_to_x32b2, r, q);
	*s = q & ~0x0606060606060606;
	x32b2 ^= x32b2 & 0xc0c0c0c0c0c0c0c0;
	return (x32b2 << 2) | q >> 1;
}

/* reverse complement
 */
inline uint64_t
x32b2_rev(uint64_t dna)
{
	dna = bit_swap_2(dna);
	dna = bit_swap_4(dna);
	return bswap(dna);
}

/* reverse complement
 */
inline uint64_t
x32b2_rc(uint64_t dna)
{
	dna = bit_swap_2(dna);
	dna = bit_swap_4(dna);
	return bswap(dna) ^ 0xaaaaaaaaaaaaaaaa;
}

/* Converts a 2bit into a sequence specific number (rcpx) that has the same high
 * bits when called with dna template 2bit as when called with its reverse
 * complement 2bit. x32b2_rcpx() is self-reversible. When called with a rcpx,
 * it returns the original 2bit.
 */
inline uint64_t
x32b2_rcpx(uint64_t dna)
{
	uint64_t rc = x32b2_rc(dna);

	// A- T- C- A- T - T- C- G- C- G => template
	// 00-10-01-00-10 - 10-01-11-01-11 template 2bit bits
	// 01-11-01-11-00 - 00-10-11-00-10 revcmp 2bit bits
	// C- G- C- G- A - A- T- G- A- T => revcmp
	//
	// 01-01-00-11-10 - 10-11-00-01-01 result of xor
	// [-----> redundant: first part is same, but per 2 bits reversed
	//
	// Store the template part, TCGCG, the rcpx is reversed to 2bit by x32b2_rcpx(rcpx).
	// m: 0xffffffff00000000 ... (e.g. 0xfebaffff00005410) ... 0xaaaaaaaa55555555
	uint64_t m = 0xffffffff00000000;

	return dna ^ (rc & m);
}
 /* The xor revcomp operation causes complementary DNA to have the same rcpx high
	* bits and order nearby. The lower post-xor 2bit, [AC] or [TG], is not order
	* important.
	*
	* Also little significant it is if two sequences differ in just one Nts
	* gray code has minimal change
	*/


// TODO: try reordering bits so that change and actual sequence are next to one
// another
inline uint64_t
x32b2_rcpx2(uint64_t dna)
{
	uint64_t rc, t;
	rc = x32b2_rc(dna);
	t = rc & 0xffffffff00000000;
	dna ^= t;
	t = (dna & t & -t) == 0;		// one if rightmost of differing high bits is set.
	dna ^= -t & (rc ^ dna) & 0xffffffff;	// flip differing bits if rightmost was set
	dna ^= (dna & 1) ^ t;			// first bit always reflects this

	return dna;
}

inline uint64_t
x32b2_rev_rcpx2(uint64_t dna)
{
	uint64_t x, t, q = dna & 1;	// q: did i flip all deviating bits?
	x = dna & 0xffffffff00000000;	// the deviating bits
	t = x & -x & dna;		// the rightmost of differing high bits
	t = q ^ (t == 0);		// whether the first bit was flipped
	dna ^= t;			// if so flip it back
	x = x32b2_rev(dna ^ (-!q & x)) ^ (-q & x); // depending on q..
	dna ^= x ^ 0xaaaaaaaa00000000;

	return dna;
}

//TODO: for multiple x32b2 fragments we could do this in a smarter way.
inline unsigned
x32b2_GC_content(uint64_t dna)
{
	return bit_count(dna & 0x5555555555555555);
}

#endif
