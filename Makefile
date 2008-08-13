CC	= gcc
CFLAGS	= -Os -Wall -DVERSION=\"$(VERSION)\" -DWHOAMI=\"`whoami`@`hostname`\"
LD	= gcc
LDFLAGS	=
LIBS	=
VERSION	= 0.5-pre
PKG	= finit-$(VERSION)
DFILES	= Makefile README finit.c finit-mod.c finit-alt.c helpers.c helpers.h \
	  patches/*patch contrib/services.sh*
BINS	= finit-mod finit-alt finit-mdv #finit-exb



#### Configurable parameters

# Default user for finit-alt if not "user"
#DEFUSER = fred

# Use -march=pentium-m to build for Eeepc
CFLAGS += -march=pentium-m

# Omit the first /sbin/hwclock if CONFIG_RTC_HCTOSYS is enabled in the kernel 
#CFLAGS += -DNO_HCTOSYS

# Append -directisa to /sbin/hwclock lines if the user has disabled
# CONFIG_GENRTC and enabled CONFIG_RTC but not enabled CONFIG_HPET_TIMER
# and CONFIG_HPET_RTC_IRQ.
#CFLAGS += -DDIRECTISA

# Use built-in run-parts instead of /bin/run-parts
CFLAGS += -DBUILTIN_RUNPARTS

#### End of configurable parameters



ifneq ($(DEFUSER),)
CFLAGS += -DDEFUSER=\"$(DEFUSER)\"
endif

.c.o:
	$(CC) -c $(CFLAGS) -o $*.o $<

all: $(BINS)

finit: finit.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mod: finit-mod.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-alt: finit-alt.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mdv: finit-mdv.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-mdv.o: finit-alt.c Makefile
	$(CC) -c $(CFLAGS) -DDIST_MDV -o $@ finit-alt.c

finit-exb: finit-exb.o helpers.o
	$(LD) $(LDFLAGS) -o $@ $+ $(LIBS)
	strip $@

finit-exb.o: finit-alt.c Makefile
	$(CC) -c $(CFLAGS) -DDIST_EEEXUBUNTU -o $@ finit-alt.c

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

