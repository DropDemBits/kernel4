#!/bin/bash
set -e

if [ $# -ne 1 ]; then
    echo "Error: No architechture specified"
    exit 1
fi

TARGET_ARCH=$1

cmake -E make_directory build/$TARGET_ARCH
cmake -E chdir build/$TARGET_ARCH cmake ../../ -DTARGET_ARCH=$TARGET_ARCH -DCMAKE_TOOLCHAIN_FILE=toolchains/toolchain-cross.cmake
cmake -E chdir build/$TARGET_ARCH make
