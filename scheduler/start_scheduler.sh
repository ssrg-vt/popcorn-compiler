#!/bin/bash

#System Configuration(s)
#export HERMIT_BOARD_NAMES="potato0 potato1 potato2"
#export HERMIT_BOARD_NAMES=""
export HERMIT_NB_BOARD=1
export HERMIT_ON_DEMANDE_MIGRATION=0

#Experiments configuration
export HERMIT_EXPERIMENTS_DIR="/tmp/hermit-scheduler/"

mix="ep cg kmeans lu ua"


#for logging info 
timestamp=$(date +%s)
RDIR=reports/$timestamp/
mkdir -p $RDIR

#copy used scripts
cp $0 $RDIR
cp scheduler.py $RDIR

#clean old experiments if any
function clean()
{
	killall perf # TODO:should be cleaned by the scheduler.py
	rm -fr $HERMIT_EXPERIMENTS_DIR/*
	for board in $HERMIT_BOARD_NAMES
	do
		echo $board
		ssh $board "rm -fr $HERMIT_EXPERIMENTS_DIR/*"
		ssh $board "killall proxy"
	done
}

#Run the experiments using scheduler.py script
#Arguments are:
#1) number of core on the board
#2) number of core on the server
#3) application list
#4) duration of the experiments
function startexperiment()
{
	clean
	timestamp=$(date +%s)
	export HERMIT_BOARD_NB_CORE=$1
	export HERMIT_SERVER_NB_CORE=$2
	echo NB_CORE_BOARD: $HERMIT_BOARD_NB_CORE > $RDIR/report.$timestamp.txt
	echo NB_CORE_SERVER: $HERMIT_SERVER_NB_CORE >> $RDIR/report.$timestamp.txt
	echo APPLICATIONS: $3 >> $RDIR/report.$timestamp.txt
	echo DURATION: $4 >> $RDIR/report.$timestamp.txt
	python -u ./scheduler.py "$3" $4 >> $RDIR/report.$timestamp.txt 2>$RDIR/err.$timestamp.txt
}

export HERMIT_BOARD_NAMES=""
#startexperiment 3 3 "$mix" 4600
#startexperiment 3 3 "ep" 3600
#startexperiment 3 3 "cg" 4400
startexperiment 3 3 "ep cg" 4500

export HERMIT_BOARD_NAMES="potato0"
#startexperiment 3 3 "ep" 3600
#startexperiment 3 3 "cg" 4400
#startexperiment 3 3 "ep cg" 4300
startexperiment 3 3 "ep cg" 4500

export HERMIT_BOARD_NAMES="potato0 potato1 potato2"
#startexperiment 3 3 "ep" 900
#startexperiment 3 3 "cg" 4500
startexperiment 3 3 "ep cg" 4500
#startexperiment 3 3 "ep cg" 4000




export HERMIT_BOARD_NAMES=""
#startexperiment 3 3 "$mix" 7200

export HERMIT_BOARD_NAMES="potato0"
#startexperiment 3 3 "$mix" 7200

export HERMIT_BOARD_NAMES="potato0 potato1 potato2"
#startexperiment 3 3 "$mix" 7200
