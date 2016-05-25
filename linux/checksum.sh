#!/bin/bash
set -e
sdb -d root on
sdb push system_kernel.tar /opt/storage/sdcard/tizen-recovery.tar
sdb pull /opt/storage/sdcard/tizen-recovery.tar system_kernel.tar.chk
MD5A=($(md5sum system_kernel.tar))
MD5B=($(md5sum system_kernel.tar.chk))
if [ "$MD5A" != "$MD5B" ];
then
	echo "MD5 sum check mismatch";
	echo $MD5A;
	echo $MD5B;
	exit 1;
else
	echo "MD5 sum check matched";
fi
sdb shell reboot recovery
