#!/bin/bash
echo -e "\n\t--------------Read My ELF to File Script---------------\n"
echo -e "\t----OPTIONS: -------------\n"
echo -e "\t----0: Readelf of glibc binarys to file-------------\n"
echo -e "\t----1: Readelf of newlib binarys to file -------------\n"
echo -e "\t----USAGE: ./readMyElfToFile.sh {option}  -------------\n"

# $1 is option
# $2 is glibc or newlib option

WHICH_BIN=$1

if [ $WHICH_BIN -eq 1 ] ; then
readelf -SW x86exe_newlib > readelf_x86n.txt
readelf -SW ARMexe_newlib > readelf_aRMn.txt

elif [ $WHICH_BIN -eq 2 ] ; then
readelf -SW x86BinGold_newlib > readelf_x86n.txt
readelf -SW armBinGold_newlib > readelf_aRMn.txt

elif [ $WHICH_BIN -eq 3 ] ; then
readelf -SW x86exe_musl > readelf_x86n.txt
readelf -SW armexe_musl > readelf_aRMn.txt

elif [ $WHICH_BIN -eq 4 ] ; then
readelf -SW x86BinGold_musl > readelf_x86n.txt
readelf -SW armBinGold_musl > readelf_aRMn.txt

else
readelf -SW x86exe > readelf_x86.txt
readelf -SW ARMexe > readelf_aRM.txt
fi

