CC	= gcc
CFLAGS	= -Os
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
