#!/bin/bash
# Version 0.2.0
echo -e "\n\t --------------- link_armObjs_To Binary VIA <<<MUSL + libBOMP>>> ----------------\n"
echo -e "Usage:\t ./mlink_armObjs.sh {MODE} {Input file w/ obj files list}\n"
echo -e "Usage:\t MODE: 0=Vanilla, 1=ALIGNED\n"

MODE=$1
INPUT=$(<$2)

if [ $MODE -eq 0 ] ; then
  OUTPUT="--output armexe_musl"
  LINKER="--script aarch64linux.x"
  MAP="-Map map_aarch.txt"
elif [ $MODE -eq 1 ] ; then
  OUTPUT="--output armBinGold_musl"
  LINKER="--script modified__aarch64.x"
  MAP="-Map map_aarchgold.txt"
else
  echo -e "\n\nCHOOSE A VALID LINKING MODE FOR PARAM 1 !!!!\n\n"
  exit 1
fi

GCC_LOC="-L/usr/lib/gcc-cross/aarch64-linux-gnu/4.8"
GCC_LIBS="-lgcc -lgcc_eh"

POPCORN="/usr/local/popcorn"

$POPCORN/bin/ld.gold -static ${OUTPUT} ${INPUT} $GCC_LOC \
 -z relro --hash-style=gnu --build-id -m aarch64linux \
 $POPCORN/aarch64/lib/crt1.o \
 $POPCORN/aarch64/lib/libc.a \
 $POPCORN/aarch64/lib/libmigrate.a \
 $POPCORN/aarch64/lib/libstack-transform.a \
 $POPCORN/aarch64/lib/libelf.a \
 $POPCORN/aarch64/lib/libpthread.a \
 $POPCORN/aarch64/lib/libbomp.a \
 $POPCORN/aarch64/lib/libc.a \
 $POPCORN/aarch64/lib/libm.a \
 --start-group $GCC_LIBS --end-group $MAP $LINKER > out_aarch64.txt || exit 1
