#!/bin/bash

#Useful functions and env variables

export HERMIT_BOARD_NAME="potato0"
export HERMIT_INSTALL_FOLDER="$HOME/hermit-popcorn/"
export HERMIT_EXPERIMENTS_DIR="/tmp/hermit-scheduler/"
export PROXY_BIN_ARM="$HERMIT_INSTALL_FOLDER/aarch64-hermit/bin/proxy"
	
APP_FOLDER="../apps/het/"

function prepare_applications()
{
	#npb_app="bt  cg  dc  ep  ft  is  lu  mg  sp  ua"
	npb_app="ep"

	mkdir -p bins

	for d in $npb_app 
	do
		dir=$APP_FOLDER/npb-$d
		#compile
		cd $dir
		ln -fs npbparams-B.h npbparams.h
		make >compile.out 2>compile.err
		cd -

		#copy
		mkdir -p bins/$d
		cp $dir/prog_*aligned bins/$d/
	done
}


function copy_proxy()
{
    rsync --no-whole-file $PROXY_BIN_ARM $HERMIT_BOARD_NAME:~/
}

function mount_tmpfs()
{
	mkdir -p $HERMIT_EXPERIMENTS_DIR	
	sudo mount -t tmpfs tmpfs /tmp/hermit-scheduler
	ssh $HERMIT_BOARD_NAME mkdir -p $HERMIT_EXPERIMENTS_DIR	
	ssh $HERMIT_BOARD_NAME "sudo mount -t tmpfs tmpfs /tmp/hermit-scheduler" #FIXME: sudo pasword
}
		
function install_tools()
{
	sudo apt install linux-tools-$(uname -r)
}
