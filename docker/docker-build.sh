#!/bin/bash

rm -rf .popcorn-compiler /tmp/.popcorn-compiler-docker-tmp

cp -a ../ /tmp/.popcorn-compiler-docker-tmp
mv /tmp/.popcorn-compiler-docker-tmp ./.popcorn-compiler

docker build -t popcorn-compiler .

rm -rf .popcorn-compiler /tmp/.popcorn-compiler-docker-tmp
