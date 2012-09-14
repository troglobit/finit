CC	= gcc
CFLAGS	= -Os -Wall -DVERSION=\"$(VERSION)\" -DWHOAMI=\"`whoami`@`hostname`\"
LD	= gcc
LDFLAGS	=
LIBS	=
VERSION	= 1.0-pre
PKG	= finit-$(VERSION)
DFILES	= Makefile README finit.c finit-mod.c finit-org.c helpers.c helpers.h \
	  patches/*patch contrib/services.sh* contrib/Makefile.*
BINS	= finit finit-mod finit-mdv #finit-exb



#### Configurable parameters

# Default user for finit if not "user"
#DEFUSER = fred

# Disable annoying gcc warning
CFLAGS += -U_FORTIFY_SOURCE

# Use -march=pentium-m to build for Eeepc
#CFLAGS += -march=pentium-m

# Omit the first /sbin/hwclock if CONFIG_RTC_HCTOSYS is enabled in the kernel 
#CFLAGS += -DNO_HCTOSYS

# Use built-in run-parts instead of /bin/run-parts
CFLAGS += -DBUILTIN_RUNPARTS

#### End of configurable parameters



ifneq ($(DEFUSER),)
CFLAGS += -DDEFUSER=\"$(DEFUSER)\"
endif

.c.o:
	$(CC) -c $(CFLAGS) -o $*.o $<

all: $(BINS)

finit-org: finit-org.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mod: finit-mod.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit: finit.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mdv: finit-mdv.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mdv.o: finit.c Makefile
	$(CC) -c $(CFLAGS) -DDIST_MDV -o $@ finit.c

finit-exb: finit-exb.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-exb.o: finit.c Makefile
	$(CC) -c $(CFLAGS) -DDIST_EEEXUBUNTU -o $@ finit.c

clean:
	rm -f *.o core *~ $(BINS)

finit.o: finit.c Makefile

finit-mod.o: finit-mod.c Makefile

helpers.o: helpers.c  Makefile

dist:
	rm -Rf $(PKG)
	mkdir -p $(PKG)/contrib $(PKG)/patches
	for i in $(DFILES); do cp $$i $(PKG)/`dirname $$i`; done
	tar cf - $(PKG) | gzip -c > $(PKG).tar.gz
	rm -Rf $(PKG)
	ls -l $(PKG).tar.gz

