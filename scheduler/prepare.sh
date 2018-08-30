#!/bin/bash

APP_FOLDER="../apps/het/"
npb_app="bt  cg  dc  ep  ft  is  lu  mg  sp  ua"

mkdir -p bins

for d in $npb_app 
do
	dir=$APP_FOLDER$d
	#compile
	cd $dir
	ln -fs npbparams-B.h npbparams.h
	make >compile.out 2>compile.err
	cd -

	#copy
	mkdir -p bins/$d
	cp $dir/prog_*aligned bins/$d/
done
	
