##
# Dockerfile for popcorn-compiler
#
# This file builds the popcorn-compiler for popcorn-kernel v5.2 using the
# Ubuntu18.04 as the base image.
#
# The clang/LLVM compiler lives at /usr/local/popcorn/bin/clang
# The musl wrapper for LLVM is at /usr/local/popcorn/x86_64/bin/musl-clang
# 
# Build application code (located in ./code):
# docker run --rm -v $(pwd)/app:/code/app <image id> make -C /code/app
##

FROM ubuntu:bionic
RUN apt-get update -y && apt-get install -y --no-install-recommends \
  bison cmake flex g++ gcc git zip make patch texinfo \
  python3 ca-certificates libelf-dev gcc-aarch64-linux-gnu
RUN apt-get install -y python-minimal 

WORKDIR /code
RUN git clone https://github.com/ssrg-vt/popcorn-compiler -b main --depth 1

WORKDIR /code/popcorn-compiler
RUN ./install_compiler.py --install-all --with-popcorn-kernel-5_2 \
  --libmigration-type=signal_trigger && rm -rf /usr/local/popcorn/src

## Use signal 35 to trigger the migration
## kill -35 $(pidof <popcorn bin>)
#RUN ./install_compiler.py --install-migration --with-popcorn-kernel-5_2 --libmigration-type=signal_trigger
