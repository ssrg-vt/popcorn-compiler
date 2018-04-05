#!/bin/bash

# cleanup
make -C lib clean &> /dev/null
make distclean &> /dev/null
rm -rf prefix

POPCORN_INSTALL=/usr/local/popcorn/
HERMIT_PREFIX=/home/usr/hermit/

export CFLAGS="-O3 -ffunction-sections -fdata-sections"
#export CC="$POPCORN_INSTALL/bin/clang"

./configure --prefix=$PWD/prefix --enable-compat --enable-elf64 \
	--disable-shared --enable-extended-format

ln -sf $PWD/lib/Makefile.hermit $PWD/lib/Makefile
make -C lib
