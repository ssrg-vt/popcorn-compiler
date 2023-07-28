#!/bin/bash

#Useful functions and env variables

#export HERMIT_BOARD_NAMES="libre@10.1.1.41"
#export HERMIT_BOARD_NAMES="sandeep@10.1.1.209"
#export HERMIT_BOARD_NAMES="sandeep@10.1.1.222"
#export HERMIT_BOARD_NAMES="sandeep@10.1.1.196"
export HERMIT_BOARD_NAMES="sandeep@10.1.1.222 sandeep@10.1.1.209 sandeep@10.1.1.196"
#export HERMIT_INSTALL_FOLDER="$HOME/hermit-popcorn/"
export HERMIT_INSTALL_FOLDER="$HOME/Scheduler_hermit-popcorn/"
export HERMIT_EXPERIMENTS_DIR="/tmp/hermit-scheduler/"
export PROXY_BIN_ARM="$HERMIT_INSTALL_FOLDER/aarch64-hermit/bin/proxy"
	
APP_FOLDER="../apps/het/"

function prepare_applications()
#startexperiment 2 2 "ep" 600
{
	mkdir -p bins
	npb_app="bt  cg  dc  ep  is  lu  mg  sp  ua"
	for d in $npb_app 
	do
		dir=$APP_FOLDER/npb-$d
		#compile
		cd $dir
		ln -fs npbparams-B.h npbparams.h
		make $clean  >compile.out 2>compile.err
		make   >compile.out 2>compile.err
		cd -

		#copy
		mkdir -p bins/$d
		cp $dir/prog_*aligned bins/$d/
	done
	other_app="microbench blackscholes  dhrystone linpack whetstone phoenix-kmeans phoenix-pca"
	for d in $other_app 
	do
		dir=$APP_FOLDER/$d

		#compile
		cd $dir
		ln -fs npbparams-B.h npbparams.h
		make $clean  >compile.out 2>compile.err
		make >compile.out 2>compile.err
		cd -

		#copy
		mkdir -p bins/$d
		cp $dir/prog_*aligned bins/$d/
		if [ -f $dir/args.sh ]
		then
			cp $dir/args.sh bins/$d/ 
		fi
	done
}



tmpfs_size="2G"
remote_mnt_cmd="if mount | grep "$HERMIT_EXPERIMENTS_DIR" >/dev/null; then sudo mount -o size=$remote_tmpfs_size,noatime -t tmpfs tmpfs /tmp/hermit-scheduler; fi"

tmpfs_size="32G"
local_mnt_cmd="if mount | grep "$HERMIT_EXPERIMENTS_DIR" >/dev/null; then sudo mount -o size=$tmpfs_size,noatime -t tmpfs tmpfs /tmp/hermit-scheduler; fi"

function __mount_tmpfs()
{
	mkdir -p $HERMIT_EXPERIMENTS_DIR	
	ssh localhost $local_mnt_cmd
}
		
function __install_tools()
{
	sudo apt install linux-tools-$(uname -r)
}

function prepare_server()
{
	__install_tools
	__mount_tmpfs
	clean="" 
	prepare_applications
}

function __copy_proxy()
{
    rsync --no-whole-file $PROXY_BIN_ARM $board:~/
}

function __mount_tmpfs_remote()
{
	ssh $board mkdir -p $HERMIT_EXPERIMENTS_DIR	
	ssh -t $board "$remote_mnt_cmd"
}

function prepare_board()
{
	for board in $HERMIT_BOARD_NAMES
	do
		echo $board
		__copy_proxy
		__mount_tmpfs_remote
	done
}
