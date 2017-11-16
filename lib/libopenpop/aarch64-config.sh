#!/bin/bash

POPCORN=/usr/local/popcorn
POPCORN_ARM64=$POPCORN/aarch64
export CC=$POPCORN/bin/clang
#export CC=aarch64-linux-gnu-gcc
#export AR=$POPCORN/bin/popcorn-ar
export TARGET="--host=aarch64-linux-gnu"
export CFLAGS="-Wno-error -ffunction-sections -fdata-sections -target aarch64-linux-gnu"

#export CFLAGS="-Wno-error -target aarch64-linux-gnu -popcorn-alignment"
#export CFLAGS="-target aarch64-linux-gnu"
#Other CFLAGS are set in Makefile.in so that configure execute without problem
#export LDFLAGS="-target aarch64-linux-gnu"
./configure --prefix=$POPCORN_ARM64 $TARGET --enable-static  --disable-shared
