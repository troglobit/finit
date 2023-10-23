EXTRA_DIST        = 10-hotplug.conf.in
systemdir         = $(system_path)
system_DATA       =
dist_system_DATA  =

if BUILD_HOTPLUG_PLUGIN
system_DATA      += 10-hotplug.conf
endif

if BUILD_TESTSERV_PLUGIN
dist_system_DATA += 90-testserv.conf
endif
