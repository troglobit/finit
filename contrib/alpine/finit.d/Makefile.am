SUBDIRS    = available
docsdir   := @docdir@/contrib/alpine/finit.d
d_LINKS    = dropbear.conf getty.conf keymap.conf modules.conf klogd.conf \
	     syslogd.conf

install-exec-hook:
	@$(MKDIR_P) $(DESTDIR)$(docsdir)/available
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
