#!/bin/bash

echo -e "\n\t --------------- nmScript.sh ----------------"
echo -e "Usage:\t ./nmScript.sh {MODE}"
echo -e "Usage:\t MODE: 0=Original, 1=BinGold 2=exe_newlib 3=BinGold_newlib\n"

MODE=$1

NM_AARCH64=aarch64-linux-gnu-nm
NM_X86=nm

if [ $MODE -eq 0 ] ; then
        echo -e "MODE $MODE exe_musl selected \n"
        $NM_X86 --numeric-sort -S x86exe_musl > nm_x86_64_musl.txt
        $NM_AARCH64 --numeric-sort -S armexe_musl > nm_aarch64_musl.txt
elif [ $MODE -eq 4 ] ; then
        echo -e "MODE $MODE BinGold_musl selected \n"
        $NM_X86 --numeric-sort -S x86BinGold_musl > nm_x86_64_musl.txt
        $NM_AARCH64 --numeric-sort -S armBinGold_musl > nm_aarch64_musl.txt
#elif [ $MODE -eq 2 ] ; then
#        echo -e "MODE $MODE exe_newlib selected \n"
#        $NM_X86 --numeric-sort -S x86exe_newlib > nm_x86_64_newlib.txt
#        $NM_AARCH64 --numeric-sort -S ARMexe_newlib > nm_aarch64_newlib.txt
#elif [ $MODE -eq 3 ] ; then
#        echo -e "MODE $MODE BinGold_newlib selected \n"
#        $NM_X86 --numeric-sort -S x86BinGold_newlib > nm_x86_64_newlib.txt
#        $NM_AARCH64 --numeric-sort -S armBinGold_newlib > nm_aarch64_newlib.txt
else
        echo -e "\n\nCHOOSE A VALID LINKING MODE FOR PARAM 1 !!!!\n\n"
        exit 1
fi
