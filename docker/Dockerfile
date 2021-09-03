##
# Dockerfile for popcorn-chameleon
#
# This file builds LLVM and all the components necessary for using
# popcorn-chameleon
#
# The LLVM compiler used lives at /usr/local/popcorn/bin/clang
# The musl wrapper for LLVM is at /usr/local/popcorn/x86_64/bin/musl-clang
##


FROM ubuntu:bionic
RUN apt-get update -y && apt-get install -y --no-install-recommends \
  bison \
  ca-certificates \
  cmake \
  flex \
  g++ \
  gcc \
  gcc-multilib \
  git \
  libelf-dev \
  python3 \
  wget \
  zip \
  ;
RUN apt install -y gcc-aarch64-linux-gnu make patch texinfo python-minimal file
RUN apt install -y libprotobuf-dev libprotobuf-c-dev protobuf-c-compiler protobuf-compiler python-protobuf

# DynamoRIO
WORKDIR /code
RUN wget https://github.com/DynamoRIO/dynamorio/releases/download/release_7_0_0_rc1/DynamoRIO-Linux-7.0.0-RC1.tar.gz
RUN tar xvf DynamoRIO-Linux-7.0.0-RC1.tar.gz
RUN mkdir -p /usr/local/popcorn
RUN cp -r DynamoRIO-Linux-7.0.0-RC1/* /usr/local/popcorn

# Popcorn compiler
WORKDIR /code
RUN git clone https://github.com/xjtuwxg/popcorn-compiler -b security --depth 1

WORKDIR /code/popcorn-compiler
RUN ./install_compiler.py --install-all --install-path /usr/local/popcorn --chameleon \
  && rm -rf /usr/local/popcorn/src

# Download chameleon
WORKDIR /code
RUN git clone --recursive https://github.com/ssrg-vt/popcorn-chameleon

WORKDIR /code/popcorn-chameleon
RUN ./util/install-compel.sh -c ./criu -d
#RUN ./util/install-compel.sh -c ./criu -i /usr/local/popcorn -d

RUN mkdir build
WORKDIR /code/popcorn-chameleon/build

RUN cmake -DCMAKE_BUILD_TYPE=Debug \
  -DDYNAMORIO_INSTALL_DIR=/code/DynamoRIO-Linux-7.0.0-RC1 \
  -DCOMPEL_INSTALL_DIR=/usr/local/chameleon \
  -DPOPCORN_INSTALL_DIR=/usr/local/popcorn \
  ..
RUN make

RUN sed -i 's/\/usr\/local\/secure-popcorn/\/usr\/local\/popcorn/'\
  /code/popcorn-chameleon/util/prepare-executable.sh
