#!/bin/bash

#System Configuration(s)
export HERMIT_BOARD_NAME=potato0

#Experiments configuration
export HERMIT_EXPERIMENTS_DIR="/tmp/hermit-scheduler/"


#for logging info 
mkdir -p reports

#clean old experiments if any
function clean()
{
	killall perf # TODO:should be cleaned by the scheduler.py
	rm -fr $HERMIT_EXPERIMENTS_DIR/*
	ssh $HERMIT_BOARD_NAME "rm -fr $HERMIT_EXPERIMENTS_DIR/*"
	ssh $HERMIT_BOARD_NAME "killall proxy"
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
	echo NB_CORE_BOARD: $HERMIT_BOARD_NB_CORE > reports/report.$timestamp.txt
	echo NB_CORE_SERVER: $HERMIT_SERVER_NB_CORE >> reports/report.$timestamp.txt
	echo APPLICATIONS: $3 >> reports/report.$timestamp.txt
	echo DURATION: $4 >> reports/report.$timestamp.txt
	python -u ./scheduler.py "$3" $4 >> reports/report.$timestamp.txt 2>reports/err.$timestamp.txt
}


#example
#startexperiment 2 2 "ep" 600
#startexperiment 3 3 "ep" 600
startexperiment 3 3 "ep cg" 4000
