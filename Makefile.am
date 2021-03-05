ACLOCAL_AMFLAGS    = -I m4
SUBDIRS            = plugins src test
doc_DATA           = README.md LICENSE contrib/finit.conf
EXTRA_DIST         = README.md LICENSE ChangeLog.md contrib/finit.conf

if CONTRIB
SUBDIRS           += contrib
endif
if DOC
SUBDIRS           += doc
endif

# Compat hook
install-dev:
	@make -C src install-pkgincludeHEADERS

# Target to run when building a release
release: distcheck
	@for file in $(DIST_ARCHIVES); do		\
		md5sum $$file > ../$$file.md5;		\
		sha256sum $$file > ../$$file.sha256;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "$$file    \tDistribution tarball\n";			\
		printf "$$file.md5\t";    cat ../$$file.md5    | cut -f1 -d' ';	\
		printf "$$file.sha256\t"; cat ../$$file.sha256 | cut -f1 -d' ';	\
	done

run:
	srcdir=@srcdir@/test \
	top_builddir=@top_builddir@ \
	TENV_ROOT=$(tenv_root) \
		@srcdir@/test/setup-root.sh

	mkdir -p $(dir $(tenv_root)/@FINIT_CONF@)
	echo 'tty [12345] @console linux noclear nologin' > $(tenv_root)/@FINIT_CONF@

	-TENV_ROOT=$(tenv_root) \
	 TESTENV_PATH=@sbindir@:/bin \
	 FINIT_RCSD=@FINIT_RCSD@ \
		test/tenv/start.sh finit

run : tenv_root ?= @abs_top_builddir@/run-rootfs
