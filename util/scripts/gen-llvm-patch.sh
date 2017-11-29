#!/bin/bash

LLVM_SRC="n/a"
CLANG_SRC="n/a"
CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
LLVM_PATCH=$(readlink -f $CUR_DIR/../../patches/llvm)

function print_help {
  echo "Generate a clang/LLVM patch for the Popcorn compiler toolchain"
  echo
  echo "Usage: gen-llvm-patch.sh [ OPTIONS ]"
  echo "Options"
  echo "  -h | --help : print help & exit"
  echo "  -s source   : LLVM source directory (required)"
  echo
  echo "Note: we assume the clang source is at <LLVM src>/tools/clang"
  echo "Note: to track new source files, you must add them to the svn index" \
       "using 'svn add'"
}

function sanity_check {
  local llvm_src=$1
  local clang_src=$2

  if [ "$llvm_src" == "n/a" ] || [ "$clang_src" == "n/a" ]; then
    echo "Please supply the LLVM source root directory!"
    print_help
    exit 1
  elif [ ! -d "$llvm_src" ] || [ ! -e "$llvm_src/LICENSE.TXT" ]; then
    echo "Invalid LLVM source directory: '$llvm_src'"
    exit 1
  elif [ ! -d "$clang_src" ] || [ ! -e "$clang_src/LICENSE.TXT" ]; then
    echo "Invalid clang source directory: '$clang_src'"
    exit 1
  fi
}

function get_llvm_version {
  local llc_bin=$1/build/bin/llc
  if [ ! -f $llc_bin ]; then
    echo "3.7.1"
  fi
  local version_line="$($llc_bin -version | grep "LLVM version")"
  local strarray=($version_line)
  echo ${strarray[2]}
}

function copy_new_files {
  local src=$1
  local dest=$2

  cd $src
  local files=$(svn status | grep "A  " | sed -e 's/A\s\+//g')
  for f in $files; do
    echo "  Copying new Popcorn file '$(basename $f)'"
    cp -f $src/$f $dest/$f
  done
  cd - > /dev/null
}

function gen_compare_patch {
  local patch_dir=$1
  local patch_name=$2
  local patch_src=$3
  local src_dir=$4
  local cur_patch=$patch_dir/${patch_name}.patch
  local new_patch=$patch_dir/${patch_name}_new.patch
  local old_patch=$patch_dir/${patch_name}_old.patch

  cd $src_dir && svn diff > $new_patch
  if [ -f $cur_patch ]; then
    if [ "$(diff $cur_patch $new_patch)" != "" ]; then
      echo "Changes between:"
      echo "  $cur_patch"
      echo "  $new_patch"
      echo "Retaining old patch in '$old_patch'"
      mv $cur_patch $old_patch
      mv $new_patch $cur_patch
      copy_new_files $src_dir $patch_src
    else
      echo "No changes to '$cur_patch'"
      rm $new_patch
    fi
  else
    echo "Creating patch '$cur_patch'"
    mv $new_patch $cur_patch
  fi
  echo
}

while [ "$1" != "" ]; do
  case $1 in
    -s)
      LLVM_SRC=$(readlink -f $2)
      CLANG_SRC=$(readlink -f $2)/tools/clang
      shift ;;
    -h | --help)
      print_help
      exit 0 ;;
  esac
  shift
done
sanity_check $LLVM_SRC $CLANG_SRC

# Get version number
LLVM_VER=$(get_llvm_version $LLVM_SRC)
LLVM=llvm-$LLVM_VER
CLANG=clang-$LLVM_VER

echo "Generating patches for clang/LLVM v$LLVM_VER"

# Generate LLVM patch
gen_compare_patch $LLVM_PATCH $LLVM $LLVM_PATCH/src $LLVM_SRC

# Generate clang patch
gen_compare_patch $LLVM_PATCH $CLANG $LLVM_PATCH/src/tools/clang $CLANG_SRC

