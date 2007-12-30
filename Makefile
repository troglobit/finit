CC	= gcc
CFLAGS	= -Os -Wall #-mno-push-args
LD	= gcc
LDFLAGS	=
LIBS	=

.o.c:
	$(CC) -c $(CFLAGS) -o $*.o $<

all: finit


finit: finit.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

clean:
	rm -f *.o core *~

finit.o: finit.c Makefile
