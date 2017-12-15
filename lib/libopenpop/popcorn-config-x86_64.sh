#!/bin/bash

POPCORN=/usr/local/popcorn
POPCORN_x86_64=$POPCORN/x86_64
export CC=$POPCORN/bin/clang
#CFLAGS are set in Makefile.in so that configure execute without problem
./configure --prefix=$POPCORN_x86_64  --enable-static  --disable-shared
