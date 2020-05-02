# N.B. to compile for 32-bit systems:
#  besides setting archflag as below, you need to edit tusl.h
#archflag := -m32
archflag :=

CFLAGS := -Wall -g2 -O2 $(archflag) -fno-strict-aliasing
LDFLAGS := $(archflag)

all: runtusl libtusl.a runansi

install: tusl.h libtusl.a tuslrc.ts
	install tusl.h /usr/local/include
	install libtusl.a /usr/local/lib
	-mkdir /usr/local/share/tusl
	install tuslrc.ts /usr/local/share/tusl

runtusl: runtusl.o tusl.o
runtusl.o: runtusl.c tusl.h 

libtusl.a: tusl.o
	ar -rs libtusl.a $<

tusl.o: tusl.c tusl.h 

runansi: runansi.o tusl.o
runansi.o: runansi.c tusl.h 

runcurst: runcurst.o tusl.o
	cc -lncurses $(LDFLAGS) $<

runcurst.o: runcurst.c tusl.h 

clean:
	rm -f *.o *.a runtusl runansi runcurst
