SUBDIRS    = available
docsdir   := @docdir@/contrib/debian/finit.d
d_LINKS    = console-setup.conf getty.conf keyboard-setup.conf	\
	     modules.conf rsyslogd.conf sshd.conf

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
