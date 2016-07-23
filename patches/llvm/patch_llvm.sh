#!/bin/bash

###############################################################################
# Config & data
###############################################################################

FILES="
include/llvm/CodeGen/AsmPrinter.h
include/llvm/CodeGen/MachineFunction.h
include/llvm/CodeGen/StackMaps.h
lib/CodeGen/AsmPrinter/AsmPrinter.cpp
lib/CodeGen/MachineFunction.cpp
lib/CodeGen/RegAllocBase.cpp
lib/CodeGen/SelectionDAG/FastISel.cpp
lib/CodeGen/SelectionDAG/SelectionDAGBuilder.cpp
lib/CodeGen/StackMaps.cpp
lib/Target/AArch64/AArch64AsmPrinter.cpp
lib/Target/X86/X86AsmPrinter.cpp
"

###############################################################################
# Utility functions
###############################################################################

function print_help {
	echo -e "patch_llvm.sh - generate & apply patches for stack transformation modifications"
	echo -e "Usage: patch_llvm.sh <gen | apply> [ OPTIONS ]\n"

	echo -e "Options for 'gen':"
	echo -e "\t-p file : Name of patch file to generate"
	echo -e "\t-d dir  : Modified LLVM source"
	echo -e "\t-c dir  : Clean LLVM source from which to generate patch\n"

	echo -e "Options for 'apply':"
	echo -e "\t-p file : Name of patch file with changes to apply"
	echo -e "\t-d dir  : Clean LLVM source to patch"
}

function die {
	echo "ERROR: $1"
	exit 1
}

function gen_patch {
	local llvm_clean=$1
	local llvm_mod=$2
	local patch_file=$3

	[ -d $llvm_clean ] || die "Cannot find clean LLVM source directory '$llvm_clean'"
 	[ -d $llvm_mod ] || die "Cannot find modified LLVM source directory '$llvm_mod'"

	echo "Generating patch '$patch_file' from modified source '$llvm_mod' against clean source '$llvm_clean'"
	rm -rf $patch_file > /dev/null 2>&1
	for f in $FILES; do
		echo -n "Generating patch for $llvm_mod/$f..."
		local diff_out=`diff -au $llvm_clean/$f $llvm_mod/$f`
		if [ "$diff_out" != "" ]; then
			echo "$diff_out" >> $patch_file
			echo "finished"
		else
			echo "skipping (empty)"
		fi
	done
}

function apply_patch {
	local llvm_mod=$1
	local patch_file=`readlink -f $2`

	[ -d $llvm_mod ] || die "Cannot find clean LLVM source directory '$llvm_mod'"

	echo "Applying patchfile $2 to '$llvm_mod'"
	patch -p6 -f -d $llvm_mod -i $patch_file
}

###############################################################################
# Driver
###############################################################################

# Command line configuration
ACTION=gen
LLVM_MOD=./llvm
LLVM_CLEAN=./llvm_clean
PATCH_FILE=llvm.patch

# Parse action (or print help if requested)
if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
	print_help
	exit 0
else
	ACTION=$1
	shift
fi

# Parse rest of arguments
while [ "$1" != "" ]; do
	case $1 in
		-h|--help)
			print_help
			exit 0 ;;
		-p)
			PATCH_FILE=$2
			shift ;;
		-d)
			LLVM_MOD=$2
			shift ;;
		-c)
			LLVM_CLEAN=$2
			shift ;;
		*)
			echo "WARNING: unknown argument '$2'" ;;
	esac
	shift
done

case $ACTION in
	gen) gen_patch $LLVM_CLEAN $LLVM_MOD $PATCH_FILE ;;
	apply) apply_patch $LLVM_MOD $PATCH_FILE ;;
	*) echo "Unknown action '$ACTION'" ;;
esac

