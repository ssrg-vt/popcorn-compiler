#!/bin/bash

POPCORN=/usr/local/popcorn
LLVM=llvm-3.7.1
CLANG=clang-3.7.1
CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
LLVM_PATCH=$CUR_DIR/../../patches/llvm

# Generate LLVM patch & compare against current patch in repo
CUR_LLVM=$LLVM_PATCH/${LLVM}.patch
NEW_LLVM=$LLVM_PATCH/${LLVM}_new.patch
OLD_LLVM=$LLVM_PATCH/${LLVM}_old.patch

function copy_new_files {
  local src=$1
  local dest=$2

  cd $src
  local files=$(svn status | grep "A  " | sed -e 's/A\s\+//g')
  for f in $files; do
    echo "  Copying new Popcorn file '$(basename $f)'"
    cp -f $src/$f $dest/$f
  done
  cd -
}

cd $POPCORN/src/$LLVM && svn diff > $NEW_LLVM
if [ -f $CUR_LLVM ]; then
  if [ "`diff $CUR_LLVM $NEW_LLVM`" != "" ]; then
    echo -e "Changes between\n  $CUR_LLVM\n  $NEW_LLVM\n\nRetaining old patch in $OLD_LLVM"
    mv $CUR_LLVM $OLD_LLVM
    mv $NEW_LLVM $CUR_LLVM
    copy_new_files $POPCORN/src/$LLVM $LLVM_PATCH/src
  else
    echo "No changes to LLVM patch"
    rm $NEW_LLVM
  fi
else
  echo "No current patch $CUR_LLVM"
fi

# Generate clang patch & compare against current patch in repo
CUR_CLANG=$LLVM_PATCH/${CLANG}.patch
NEW_CLANG=$LLVM_PATCH/${CLANG}_new.patch
OLD_CLANG=$LLVM_PATCH/${CLANG}_old.patch

cd $POPCORN/src/$LLVM/tools/clang && svn diff > $NEW_CLANG
if [ -f $CUR_CLANG ]; then
  if [ "`diff $CUR_CLANG $NEW_CLANG`" != "" ]; then
    echo -e "Changes between\n  $CUR_CLANG\n  $NEW_CLANG\n\nRetaining old patch in $OLD_CLANG"
    mv $CUR_CLANG $OLD_CLANG
    mv $NEW_CLANG $CUR_CLANG
    copy_new_files $POPCORN/src/$LLVM/tools/clang $LLVM_PATCH/src/tools/clang
  else
    echo "No changes to clang patch"
    rm $NEW_CLANG
  fi
else
  echo "No current patch $CUR_CLANG"
fi

