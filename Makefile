CFLAGS := -Wall -g2 -O2

all: runtusl

runtusl: runtusl.o tusl.o

runtusl.o: runtusl.c tusl.h 
tusl.o:	tusl.c tusl.h 

clean:
	rm -f *.o runtusl
