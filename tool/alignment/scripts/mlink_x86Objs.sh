#!/bin/bash
echo -e "\n\t --------------- link_x86Objs_To Binary VIA <<<MUSL + libBOMP>>>> ----------------\n"
echo -e "Usage:\t ./mlink_x86Objs.sh {MODE} {Input file w/ obj file list}\n"
echo -e "Usage:\t MODE: 0=Vanilla, 1=ALIGNED\n"

MODE=$1
INPUT=$(<$2)
TARGET=x86_64-linux-gnu

if [ $MODE -eq 0 ] ; then
  OUTPUT="--output x86exe_musl"
  LINKER="--script elf_x86_64.x"
  MAP="-Map map_x86.txt"
elif [ $MODE -eq 1 ] ; then
  OUTPUT="--output x86BinGold_musl"
  LINKER="--script modified__elf_x86_64.x"
  MAP="-Map map_x86gold.txt"
else
  echo -e "\n\nCHOOSE A VALID LINKING MODE FOR PARAM 1 !!!!\n\n"
  exit 1
fi

POPCORN="/usr/local/popcorn"

$POPCORN/bin/ld.gold -static \
 ${OUTPUT} ${INPUT} -z relro --hash-style=gnu --build-id -m elf_x86_64 \
 /usr/lib/gcc/x86_64-linux-gnu/4.8/libgcc.a \
 $POPCORN/x86_64/lib/crt1.o \
 $POPCORN/x86_64/lib/libc.a \
 $POPCORN/x86_64/lib/libmigrate.a \
 $POPCORN/x86_64/lib/libstack-transform.a \
 $POPCORN/x86_64/lib/libelf.a \
 $POPCORN/x86_64/lib/libpthread.a \
 $POPCORN/x86_64/lib/libpthread.a \
 $POPCORN/x86_64/lib/libc.a \
 $POPCORN/x86_64/lib/libm.a \
 --start-group --end-group $MAP $LINKER > out_x86_64.txt || exit 1
