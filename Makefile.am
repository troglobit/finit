ACLOCAL_AMFLAGS    = -I m4
SUBDIRS            = man plugins src system tmpfiles.d
dist_doc_DATA      = README.md LICENSE contrib/finit.conf

if CONTRIB
SUBDIRS           += contrib
endif
if DOC
SUBDIRS           += doc
endif
if LIBSYSTEMD
SUBDIRS           += libsystemd
endif

if LIBSYSTEMD
SUBDIRS           += test
# Explicit dependency to ensure libsystemd is built before test
test: libsystemd
else
SUBDIRS           += test
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

DISTCHECK_CONFIGURE_FLAGS = --prefix=/usr --sysconfdir=/etc --localstatedir=/var   \
			    --enable-testserv-plugin --enable-x11-common-plugin    \
			    --with-watchdog --with-keventd --with-fstab=/etc/fstab \
			    --with-bash-completion-dir=no
