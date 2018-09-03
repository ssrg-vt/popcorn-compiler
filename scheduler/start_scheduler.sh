#!/bin/bash


#clean old experiments if any
rm -fr /tmp/hermit-scheduler/
ssh potato "rm -fr /tmp/hermit-scheduler/"

#for logging info 
mkdir -p reports

function startexperiment()
{
	timestamp=$(date +%s)
	export HERMIT_BOARD_NB_CORE=$1
	export HERMIT_SERVER_NB_CORE=$2
	python -u ./scheduler.py "ep" $3 > reports/report.$timestamp.txt 2>reports/err.$timestamp.txt
}


startexperiment 2 2 800
