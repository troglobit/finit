SUBDIRS    = available
docsdir   := @docdir@/contrib/debian/finit.d
d_LINKS    = console-setup.conf getty.conf keyboard-setup.conf	\
	     modules.conf rsyslogd.conf sshd.conf

install-exec-hook:
	@$(MKDIR_P) $(DESTDIR)$(docsdir)
	@for file in $(d_LINKS); do				\
		lnk=$(DESTDIR)$(docsdir)/$$file;		\
		src=available/$$file;				\
		if [ -L $$lnk ]; then				\
			echo "Skipping $$lnk exists.";		\
		else						\
			$(LN_S) $$src $$lnk;			\
		fi;						\
	done


uninstall-hook:
	@for file in $(d_LINKS); do				\
		lnk=$(DESTDIR)$(docsdir)/$$file;		\
		src=available/$$file;				\
		if [ "`readlink $$lnk`" != "$$src" ]; then 	\
			continue; 				\
		fi;						\
		$(RM) $$lnk;					\
	done
