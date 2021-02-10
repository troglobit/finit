SUBDIRS    = available
docsdir   := @docdir@/contrib/alpine/finit.d
d_LINKS    = dropbear.conf getty.conf keymap.conf modules.conf	\
	     klogd.conf syslogd.conf

install-exec-hook:
	@$(MKDIR_P) $(DESTDIR)$(docsdir)/available
	@$(MKDIR_P) $(DESTDIR)$(docsdir)/enabled
	@for file in $(d_LINKS); do				\
		lnk=$(DESTDIR)$(docsdir)/enabled/$$file;	\
		src=available/$$file;				\
		rm -f $$lnk;					\
		$(LN_S) ../$$src $$lnk;				\
	done


uninstall-hook:
	@for file in $(d_LINKS); do				\
		lnk=$(DESTDIR)$(docsdir)/enabled/$$file;	\
		src=available/$$file;				\
		$(RM) -f $$lnk;					\
	done
