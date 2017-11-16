#!/bin/bash

LIB=libopenpop.a
install .libs/libopenpop.a /usr/local/popcorn/aarch64/lib/libopenpop.a
install .libs/libopenpop_x86_64.a /usr/local/popcorn/x86_64/lib/libopenpop.a
cp omp.h /usr/local/popcorn/x86_64/include/omp.h
cp omp.h  /usr/local/popcorn/aarch64/include/omp.h
