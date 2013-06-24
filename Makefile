CC=		gcc
CXX=		g++
CFLAGS=		-g -Wall -O2
CXXFLAGS=	$(CFLAGS)
DFLAGS=		-DHAVE_PTHREAD #-D_NO_SSE2 #-D_FILE_OFFSET_BITS=64
OBJS=		
PROG=		test_b6 # only a test for now
INCLUDES=	-lm -lz -lpthread
LIBS=		
SUBDIRS=	.

.SUFFIXES:.c .o .cc

.c.o:
		$(CC) -c $(CFLAGS) $(DFLAGS) $(INCLUDES) $< -o $@
.cc.o:
		$(CXX) -c $(CXXFLAGS) $(DFLAGS) $(INCLUDES) $< -o $@

all:$(PROG)

test_b6:$(OBJS)	test_b6.o
		$(CC) $(CFLAGS) $(DFLAGS) $(OBJS) test_b6.o -o $@ $(LIBS)

clean:
		rm -f gmon.out *.o a.out $(PROG) *~ *.a
