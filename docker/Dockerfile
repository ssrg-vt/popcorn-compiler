FROM ubuntu:16.04

RUN apt-get -qq update
RUN apt-get install -y vim build-essential gcc-aarch64-linux-gnu flex bison python python3 cmake subversion zip ssh
RUN apt-get install -y git

ENV EDITOR=vim

ADD .popcorn-compiler /root/popcorn-compiler

RUN cd /root/popcorn-compiler; ./install_compiler.py

CMD echo "Welcome to het-popcorn toolchain!"; /bin/bash
WORKDIR /root
