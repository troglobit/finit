ACLOCAL_AMFLAGS    = -I m4
SUBDIRS            = plugins src
dist_sbin_SCRIPTS  = service
doc_DATA           = README.md LICENSE finit.conf
EXTRA_DIST         = README.md LICENSE ChangeLog.md finit.conf

# Compat hook
install-dev:
	@make -C src install-pkgincludeHEADERS

# Target to run when building a release
release: dist
	@for file in $(DIST_ARCHIVES); do	\
		md5sum $$file > ../$$file.md5;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "$$file    \tDistribution tarball\n";			\
		printf "$$file.md5\t"; cat ../$$file.md5 | cut -f1 -d' ';	\
	done
