#!/bin/bash

LLVM_VER=3.7.1
LLVM_SRC="n/a"
CLANG_SRC="n/a"
CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PATCHES=$(readlink -f $CUR_DIR/../../patches/llvm)
BACKUP=$CUR_DIR/llvm-bak
NTHREADS=$(cat /proc/cpuinfo | grep processor | wc -l)
UNTRACKED_SKIP=".ycm_extra_conf.py TODO build projects/compiler-rt"

function print_help {
  echo "Re-apply clang/LLVM patches & re-install compiler"
  echo
  echo "Usage: reapply-llvm-patch.sh [ OPTIONS ]"
  echo "Options"
  echo "  -h | --help : print help & exit"
  echo "  -s source   : LLVM source directory (required)"
  echo "  -p dir      : directory containing patches"
  echo "                default: $PATCHES"
  echo
  echo "Note: we assume the clang source is at <LLVM src>/tools/clang"
  echo "Note: the compiler must have already been built/installed by" \
       "install_compiler.py"
}

function sanity_check {
  local llvm_src=$1
  local clang_src=$2
  local patch_dir=$3

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
  elif [ ! -d "$llvm_src/build" ]; then
    echo "No previous clang/LLVM installation - please install using" \
         "install_compiler.py"
    exit 1
  elif [ ! -d "$patch_dir" ]; then
    echo "No directory containing patches: '$patch_dir'"
    exit 1
  fi
}

function die {
  local msg=$1
  local src=$2
  local backup=$3
  echo "ERROR: $msg"
  if [ "$src" != "" ] && [ "$backup" != "" ]; then
    echo "Restoring from backup directory '$backup'"
    local files=$(find $backup -type f)
    for f in $files; do
      local no_prefix=${f/$backup/}
      cp $backup/$no_prefix $src/$no_prefix
    done
  fi
  exit 1
}

function reapply_patch {
  local patch=$1
  local src=$2
  local backup=$3

  if [ ! -f $patch ]; then die "No patch named '$patch'"; fi
  echo "Backing up files in '$backup'"

  # Copy all new/changed files to the backup directory, revert all modified
  # files to the svn checkout and remove all new files
  mkdir -p $backup || die "could not create backup directory"
  cd $src
  local modified=$(svn status | grep "M " | sed -e 's/M       //g')
  local added=$(svn status | grep "A " | sed -e 's/A       //g')
  local untracked=$(svn status | grep "? " | sed -e 's/?       //g')

  for f in $modified; do
    mkdir -p $backup/$(dirname $f)
    cp $f $backup/$f || die "could not create file backup" $src $backup
    svn revert $f || die "could not revert changes for '$f'" $src $backup
  done

  for f in $added; do
    echo "Removing '$f'"
    mkdir -p $backup/$(dirname $f)
    cp $f $backup/$f || die "could not create file backup" $src $backup
    rm $f || die "could not remove '$f'" $src $backup
  done

  for f in $untracked; do
    if [[ $UNTRACKED_SKIP =~ $f ]]; then
      echo "Skipping '$f'"
    else
      echo "Removing '$f'"
      mkdir -p $backup/$(dirname $f)
      cp $f $backup/$f || die "could not create file backup" $src $backup
      rm $f || die "could not remove '$f'" $src $backup
    fi
  done

  # Re-apply patch & remove backup
  patch -p0 < $patch || die "could not patch" $src $backup
  echo

  # Remove files which were previously added to the svn index but no longer
  # exist in the new patch
  local missing=$(svn status | grep "! " | sed -e 's/!       //g')
  for f in $missing; do
    $(svn rm $f)
  done
}

while [ "$1" != "" ]; do
  case $1 in
    -s)
      LLVM_SRC=$2
      CLANG_SRC=$2/tools/clang
      shift ;;
    -p)
      PATCHES=$2
      shift ;;
    -h | --help)
      print_help
      exit 0 ;;
  esac
  shift
done
sanity_check $LLVM_SRC $CLANG_SRC $PATCHES

echo "Re-applying patches for clang/LLVM"

# Re-apply LLVM/clang patches & reinstall
reapply_patch $PATCHES/llvm-${LLVM_VER}.patch $LLVM_SRC $BACKUP
reapply_patch $PATCHES/clang-${LLVM_VER}.patch $CLANG_SRC $BACKUP/tools/clang
make -j $NTHREADS -C $LLVM_SRC/build install || die "could not re-install"
rm -r $BACKUP

