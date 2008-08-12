CFLAGS := -Wall -g2 -O2
LDFLAGS := -lncurses

all: runtusl

runtusl: runtusl.o tusl.o
runtusl.o: runtusl.c tusl.h 

tusl.o: tusl.c tusl.h 

runcurst: runcurst.o tusl.o
runcurst.o: runcurst.c tusl.h 

clean:
	rm -f *.o runtusl runcurst
