#!/bin/sh

set -e
cd sysroot
find * | awk '!/(include)|(lib)|(\.[oha])/' -
