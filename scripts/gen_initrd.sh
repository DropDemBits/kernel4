#!/bin/sh

set -e
echo "Generating initrd"
cd initrd/files
tar -c --format=ustar -v -f ../initrd/initrd.tar *
