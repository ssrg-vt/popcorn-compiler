#!/bin/bash

set -e


binary=$1
_arm_ext="_aarch64"
_x86_ext="_x86-64"
_config_file=$HOME/.popcorn

arm_bin=$binary$_arm_ext
x86_bin=$binary$_x86_ext

dest_dir=$(pwd)

while read -r line; do
	mip=$(echo "$line" | cut -d ';' -f1)
 	mtype=$(echo "$line" | cut -d ';' -f2)
    	echo "machine $mip type $mtype"
	scp $arm_bin $x86_bin $mip:$dest_dir
	if [ $mtype = "X86_64" ]
	then
		scp $x86_bin $mip:$dest_dir/$binary
	elif [ $mtype = "AARCH64" ] 
	then
		scp $arm_bin $mip:$dest_dir/$binary
	else
		echo "ERROR: unkown machine"
	fi
done < "$_config_file"


