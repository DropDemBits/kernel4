#!/bin/bash

if [ $# -ne 2 ]; then
	echo "Usage: " $0 " <target architechture> <address>"
	exit 1
fi

ARCH=$1
ADDR=$2
addr2line -e build/$ARCH/kernel/k4-$ARCH.kern $ADDR
