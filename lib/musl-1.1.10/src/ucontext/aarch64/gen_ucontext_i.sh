#!/bin/bash

echo "#define _GNU_SOURCE" >tmp.c
gawk -f gen-as-const.awk ucontext_i.sym >>tmp.c
cat tmp.c | gcc -std=gnu11 -fgnu89-inline   -Wall -Werror -Wundef -Wwrite-strings -fmerge-all-constants -fno-stack-protector -frounding-math -g -Wstrict-prototypes -Wold-style-definition     -ftls-model=initial-exec   -U_FORTIFY_SOURCE -S -o out.hT3 -I../../../arch/aarch64 -I../../../src/internal -I../../../include -nostdinc -x c - 
sed -n 's/^.*@@@name@@@\([^@]*\)@@@value@@@[^0-9Xxa-fA-F-]*\([0-9Xxa-fA-F-][0-9Xxa-fA-F-]*\).*@@@end@@@.*$/#define \1 \2/p'     out.hT3 > ucontext_i.h
rm out.hT3
rm tmp.c

