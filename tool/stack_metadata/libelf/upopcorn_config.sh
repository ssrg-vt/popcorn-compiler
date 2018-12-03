#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

cd $DIR

./configure -prefix=$DIR/../ext --enable-elf64 --disable-shared --enable-extended-format
