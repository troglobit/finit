CC	= gcc
CFLAGS	= -Os -Wall -DVERSION=\"$(VERSION)\" -DWHOAMI=\"`whoami`@`hostname`\"
LD	= gcc
LDFLAGS	=
LIBS	=
VERSION	= 0.2
PKG	= finit-$(VERSION)


#### Configurable parameters

# Add -DDEBUG to build with debug messages, also drops to terminal after X
#CFLAGS += -DDEBUG

# Parameters added by Metalshark

# Use -march=pentium-m to build for Eeepc
CFLAGS += -march=pentium-m

# Omit the first /sbin/hwclock if CONFIG_RTC_HCTOSYS is enabled in the kernel 
CFLAGS += -DNO_HCTOSYS

# Append –directisa to /sbin/hwclock lines if the user has disabled
# CONFIG_GENRTC and enabled CONFIG_RTC but not enabled CONFIG_HPET_TIMER
# and CONFIG_HPET_RTC_IRQ.
#CFLAGS += -DDIRECTISA



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

dist:
	rm -Rf $(PKG)
	mkdir $(PKG)
	cp Makefile README finit.c finit-mod.c finit-alt.c $(PKG)
	tar cf - $(PKG) | gzip -c > $(PKG).tar.gz
	rm -Rf $(PKG)
	ls -l $(PKG).tar.gz
