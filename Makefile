CFLAGS := -Wall -g2 -O2
LDFLAGS := -lncurses

all: runtusl libtusl.a

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

runcurst: runcurst.o tusl.o
runcurst.o: runcurst.c tusl.h 

clean:
	rm -f *.o *.a runtusl runcurst
