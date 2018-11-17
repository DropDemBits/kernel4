#!/bin/sh

set -e
echo "Generating initrd"
mkdir -p initrd/initrd
cd initrd/files
find * | pax -w -d -M 0x008F > ../initrd/initrd.tar
