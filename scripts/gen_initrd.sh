#!/bin/sh

set -e
echo "Generating initrd"
mkdir -p initrd/initrd

cd sysroot
find * | awk '!/(include)|(lib)|(\.[oha])/' - | pax -w -Ld -M 0x008F > ../initrd/initrd/initrd.tar
tar -tf ../initrd/initrd/initrd.tar
