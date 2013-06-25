CC=		gcc
CXX=		g++
CFLAGS=		-g -Wall -O2
CXXFLAGS=	$(CFLAGS)
DFLAGS=		-DHAVE_PTHREAD #-D_NO_SSE2 #-D_FILE_OFFSET_BITS=64
OBJS=		
PROG=		fa_b2
INCLUDES=	
LIBS=		-lm -lz -lpthread
SUBDIRS=	.

.SUFFIXES:.c .o .cc

.c.o:
		$(CC) -c $(CFLAGS) $(DFLAGS) $(INCLUDES) $< -o $@
.cc.o:
		$(CXX) -c $(CXXFLAGS) $(DFLAGS) $(INCLUDES) $< -o $@

all:$(PROG)

test_b6.o:bits/revbin.h

test_b6:$(OBJS)	test_b6.o
		$(CC) $(CFLAGS) $(DFLAGS) $(OBJS) test_b6.o -o $@ $(LIBS)

fa_b2:$(OBJS)	fa_b2.o
		$(CC) $(CFLAGS) $(DFLAGS) $(OBJS) fa_b2.o -o $@ $(LIBS)

clean:
		rm -f gmon.out *.o a.out $(PROG) *~ *.a
