CC	= gcc
CFLAGS	= -Os -Wall #-mno-push-args
LD	= gcc
LDFLAGS	=
LIBS	=

.c.o:
	$(CC) -c $(CFLAGS) -o $*.o $<

all: finit finit-mdv

finit: finit.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mdv: finit-mdv.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)

finit-mdv.o: ffinit.c
	$(CC) -c $(CFLAGS) -DDIST_MDV -o $@ $+

clean:
	rm -f *.o core *~

finit.o: finit.c Makefile
