SUBDIRS    = alpine debian void
bin_SCRIPT = service
docsdir   := @docdir@/contrib
docs_DATA  = README.md finit.conf procmon.sh
EXTRA_DIST = $(docs_DATA)

if BASH
bashdir            = $(BASH_DIR)
dist_bash_DATA     = initctl
endif
