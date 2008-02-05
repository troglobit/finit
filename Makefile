CC	= gcc
CFLAGS	= -Os -Wall -DDEBUG
LD	= gcc
LDFLAGS	=
LIBS	=

.c.o:
	$(CC) -c $(CFLAGS) -o $*.o $<

all: finit finit-mod finit-mdv

%: %.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mdv.o: finit-alt.c Makefile
	$(CC) -c $(CFLAGS) -DDIST_MDV -o $@ finit-alt.c

clean:
	rm -f *.o core *~

finit.o: finit.c Makefile

finit-mod.o: finit-mod.c Makefile
