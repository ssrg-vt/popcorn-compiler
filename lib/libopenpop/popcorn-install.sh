#!/bin/bash

POPCORN=${1:-"/usr/local/popcorn"}
POPCORN_ARM64=$POPCORN/aarch64
POPCORN_x86_64=$POPCORN/x86_64

install .libs/libopenpop.a $POPCORN_ARM64/lib/libopenpop.a
install .libs/libopenpop_x86_64.a $POPCORN_x86_64/lib/libopenpop.a
cp omp.h $POPCORN_ARM64/include/omp.h
cp omp.h $POPCORN_x86_64/include/omp.h
