#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include "b6.h"

void test_b6() {
	unsigned t, i, c;
	for (i=0; i <= 6; i += 2) {
		c = b6(uc, deoxy, b2toa, i);
		t = b6(uc, deoxy, atob2, c);
		printf("%u: %c(0x%x) :%u, isb6:%u\n", i, (char)c, c, t, isb6(t));
	}
	c = 'N';
	t = b6(uc, deoxy, atob2, c);
	printf("rev: %c(0x%x) => 0x%x, isb6:%u\n", (char)c, c, t, isb6(t));
}

int main()
{
	fprintf(stderr, "\nSingle character conversion:\n");
	test_b6();
	unsigned i, j;
	const char* s = "ACTGGTGCCCTGCTCTAAGTTGACCATGTCAC";
	//const char* s = "AAAAAAAAAAAANAAAAAAAAAAAAAAAAAGA";
	fprintf(stderr, "\n32 character conversion:\n");
	fprintf(stderr, "%s\n", s);
	char q[33];
	uint64_t t[4] = {0, 0, 0, 0};
	for (j = 0; j != 4; ++j) {
		for (i = 0; i != 8; ++i) {
			t[j] |= (uint64_t)s[i + (j*8)] << (i * 8);
		}
	}
	fprintf(stderr, "Before conversion:\t\t\t0x%lx\t0x%lx\t0x%lx\t0x%lx\n", t[0], t[1], t[2], t[3]);
	uint64_t ret = atox32b2(x32uc, deoxy, &t[0]);
	fprintf(stderr, "After conversion (set for non-Nts):\t0x%lx\t0x%lx\t0x%lx\t0x%lx\n",
		t[0], t[1], t[2], t[3]);
	if (t[0] | t[1] | t[2] | t[3]) {
		printf("Got strange characters in nucleotides\n");
		return 0;
	}
	t[0] = ret;
	x32b2toa(x32uc, deoxy, &t[0]);
	fprintf(stderr, "Converted back:\t\t\t\t0x%lx\t0x%lx\t0x%lx\t0x%lx\n", t[0], t[1], t[2], t[3]);
	for (j = 0; j != 4; ++j) {
		for (i = 0; i != 8; ++i) {
			q[i+j*8] = (char)(t[j] >> (i*8));
		}
	}
	q[32] = '\0';
	fprintf(stderr, "%s\n", s);
	fprintf(stderr, "%s\n", q);

	fprintf(stderr, "\nAdding one character:\n");
	i = b6(uc, deoxy, atob2, 'G');
	ret = x32b2_add_b2(ret, i);
	t[0] = ret;
	x32b2toa(x32uc, deoxy, &t[0]);
	fprintf(stderr, "Converted back:\t\t\t\t0x%lx\t0x%lx\t0x%lx\t0x%lx\n", t[0], t[1], t[2], t[3]);
	for (j = 0; j != 4; ++j) {
		for (i = 0; i != 8; ++i) {
			q[i+j*8] = (char)(t[j] >> (i*8));
		}
	}
	q[32] = '\0';
	fprintf(stderr, "%s\n", s);
	fprintf(stderr, "%s\n", q);

	return 0;
}

