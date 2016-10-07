#!/bin/bash

POPCORN=/usr/local/popcorn
LLVM=llvm-3.7.1
CUR_DIR=`pwd`
LLVM_PATCH=$CUR_DIR/../../patches/llvm
CUR=$LLVM_PATCH/${LLVM}.patch
NEW=$LLVM_PATCH/${LLVM}_new.patch
OLD=$LLVM_PATCH/${LLVM}_old.patch

cd $POPCORN/src/$LLVM && svn diff > $NEW
if [ -f $CUR ]; then
  if [ "`diff $CUR $NEW`" != "" ]; then
    echo "Changes between $CUR and $NEW, retaining old patch in $OLD"
    mv $CUR $OLD
    mv $NEW $CUR
  else
    echo "No changes"
    rm $NEW
  fi
else
  echo "No current patch $CUR"
fi

