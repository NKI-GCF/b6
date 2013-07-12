#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include "b6.h"

void
test_b6()
{
  unsigned t, i, c;

  for (i = 0; i <= 6; i += 2) {
    c = b6(uc, deoxy, b2toa, i);
    t = b6(uc, deoxy, atob2, c);
    printf("%u: %c(0x%x) :%u, isb6:%u\n", i, (char) c, c, t, isb6(t));
  }
  c = 'N';
  t = b6(uc, deoxy, atob2, c);
  printf("rev: %c(0x%x) => 0x%x, isb6:%u\n", (char) c, c, t, isb6(t));
}

void
tr_x32b2(uint64_t b2, uint64_t o, const char *s, char q[33], const char *msg)
{
  uint64_t t[4] = { 0, 0, 0, 0 };
  t[0] = b2;
  x32b2toa(uc, deoxy, &t[0]);
  fprintf(stderr, "b2:%lx\t%s:\t%lu\n", b2, msg, bit_count(b2 ^ o));
  unsigned i, j;
  for (j = 0; j != 4; ++j) {
    for (i = 0; i != 8; ++i) {
      q[i+j*8] = (char)(t[j] >> (i*8));
    }
  }
  q[32] = '\0';
  fprintf(stderr, "%s\n", s);
  fprintf(stderr, "%s\n\n", q);
}

int
main()
{
  fprintf(stderr, "\nSingle character conversion:\n");
  test_b6();
  unsigned i, j;
  const char *s = "ACTGGTGCCTGCTCCAAGTTGACCATGTCACA";

  fprintf(stderr, "\n32 character conversion:\n");
  fprintf(stderr, "%s\n", s);
  char q[33];
  uint64_t t[4] = { 0, 0, 0, 0 };
  for (j = 0; j != 4; ++j) {
    for (i = 0; i != 8; ++i) {
      t[j] |= (uint64_t) s[i + (j * 8)] << (i * 8);
    }
  }
  /*fprintf(stderr, "Before conversion:\t\t\t0x%lx\t0x%lx\t0x%lx\t0x%lx\n",
	  t[0], t[1], t[2], t[3]);*/
  uint64_t ret = atox32b2(uc, deoxy, &t[0]);

  if (t[0] | t[1] | t[2] | t[3]) {
    printf("Got strange characters in nucleotides\n");
    return 0;
  }
  tr_x32b2(ret, ret, s, q, "Converted back");

  uint64_t rcpx;

/*	rcpx = x32b2_rcpx(ret);
	fprintf(stderr, "\ttemplate rcpx:%lx\n", rcpx);
	tr_x32b2(x32b2_rcpx(rcpx), ret, s, q, "Template, converted back from rcpx");*/
  rcpx = x32b2_rcpx(ret);
  fprintf(stderr, "\thalf template rcpx:\t%lx\n", rcpx);

  rcpx = x32b2_rcpx2(ret);
  fprintf(stderr, "\ttest template rcpx:\t%lx\n", rcpx);
  tr_x32b2(x32b2_rev_rcpx2(rcpx), ret, s, q,
	   "Template, converted back from rcpx");

  uint64_t rc = x32b2_rc(ret);

  tr_x32b2(rc, rc, s, q, "Reverse complement");
  rcpx = x32b2_rcpx(rc);
  fprintf(stderr, "\thalf revcomp rcpx:\t%lx\n", rcpx);
  rcpx = x32b2_rcpx2(rc);
  fprintf(stderr, "\ttest revcomp rcpx:\t%lx\n", rcpx);
  tr_x32b2(x32b2_rev_rcpx2(rcpx), rc, s, q,
	   "Reverse complement, converted back from rcpx");


  /*i = b6(uc, deoxy, atob2, 'G');
  ret = x32b2_add_b6(ret, i);
  tr_x32b2(ret, ret, s, q, "Added one character");
  rcpx = x32b2_rcpx2(ret);
  fprintf(stderr, "\twith char+shift, rcpx:\t%lx\n", rcpx);


  const char *z = "GGTACGTG";

  for (i = 0; i != 8; ++i) {
    t[3] |= (uint64_t) z[i] << (i * 8);
  }

  ret = x32b2_add_8a(uc, deoxy, ret, &t[3]);
  tr_x32b2(ret, ret, s, q, "Added eight characters");
  if (t[3]) {
    printf("Got strange characters in nucleotides\n");
    return 0;
  }
  rcpx = x32b2_rcpx2(ret);
  fprintf(stderr, "\twith char+shift, rcpx:\t%lx\n", rcpx);
  printf("GC content:%u\n", x32b2_GC_content(ret));*/

	ret ^= 0x1000;
  rcpx = x32b2_rcpx2(ret);
  fprintf(stderr, "\t[AC] <=> [TG] substtitution, rcpx:\t%lx\n", rcpx);
	ret ^= 0x1000;
	ret ^= 0x200000;
  rcpx = x32b2_rcpx2(ret);
  fprintf(stderr, "\t[AT] <=> [CG] substtitution, rcpx:\t%lx\n", rcpx);
	ret ^= 0x200000;
	ret ^= 0x30000000;
  rcpx = x32b2_rcpx2(ret);
  fprintf(stderr, "\ty/ACGT/GTAC/ substtitution, rcpx:\t%lx\n", rcpx);
	ret ^= 0x30000000;

  return 0;
}
