# This is a basic /etc/fstab that mounts up all of the virtual filesystems
# that older versions of Finit used to mount internally. It can serve as a
# starting point for systems running Busybox's mount implementation. Make
# sure that CONFIG_FEATURE_MOUNT_HELPERS enabled; as this is required
# create the /dev/{pts,shm} directories.

devtmpfs	/dev		devtmpfs	defaults		0	0
mkdir#-p	/dev/pts	helper		none			0	0
devpts		/dev/pts	devpts		mode=620,ptmxmode=0666	0	0
mkdir#-p	/dev/shm	helper		none			0	0
tmpfs		/dev/shm	tmpfs		mode=0777		0	0
proc		/proc		proc		defaults		0	0
tmpfs		/tmp		tmpfs		mode=1777,nosuid,nodev	0	0
tmpfs		/run		tmpfs		mode=0755,nosuid,nodev	0	0
sysfs		/sys		sysfs		defaults		0	0
