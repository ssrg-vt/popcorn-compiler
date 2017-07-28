#!/bin/bash

echo -e "Clean Directory Tool"
echo -e "Option 4: EVERYTHING"
echo -e "Option 3: all scripts, text, linker files (****.x), intermediate Bins + Objs"
echo -e "Option 2: all text, linker files (***.x), intermediate Bins + Objs"
echo -e "Option 1: all intermediate Bins + Objs"
echo -e "Option *: Nothing"

case "$1" in
	"4")
	rm x86BinGold_musl
	rm armBinGold_musl
	rm -rf popcorn/
	
# ALL linker files (*****.x)
	;&
	"3")
	rm *.sh
# ALL object files
	;&
	"2")
	rm *.txt
	rm *.x

# ALL binaries
	;&
	"1")
	rm *.o
	rm x86exe_musl
	rm armexe_musl
# ??
	;&
	* )
	;;
esac	
