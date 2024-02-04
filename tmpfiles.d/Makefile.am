finitdir              = $(finit_tmpfiles)
dist_finit_DATA       = finit.conf

tmpfilesdir           = $(tmpfiles_path)
dist_tmpfiles_DATA    = etc.conf legacy.conf var.conf
dist_tmpfiles_DATA   += dnsmasq.conf frr.conf lldpd.conf openswan.conf \
			quagga.conf sshd.conf uuidd.conf

if BUILD_TESTSERV_PLUGIN
dist_tmpfiles_DATA   += testserv.conf
endif

if BUILD_X11_COMMON_PLUGIN
dist_tmpfiles_DATA   += x11.conf
endif
