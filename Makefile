CC	= gcc
CFLAGS	= -Os -Wall -DDEBUG
LD	= gcc
LDFLAGS	=
LIBS	=

.c.o:
	$(CC) -c $(CFLAGS) -o $*.o $<

all: finit-mdv

finit: finit.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mdv: finit-mdv.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)

finit-mdv.o: ffinit.c Makefile
	$(CC) -c $(CFLAGS) -DDIST_MDV -o $@ ffinit.c

clean:
	rm -f *.o core *~

finit.o: finit.c Makefile
