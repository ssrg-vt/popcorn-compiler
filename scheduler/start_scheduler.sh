#!/bin/bash

timestamp=$(date +%s)
mkdir -p reports
rm -fr /tmp/hermit-scheduler/
export HERMIT_BOARD_NB_CORE=2
export HERMIT_SERVER_NB_CORE=2
python -u ./scheduler.py "ep" 900 > reports/report.$timestamp.txt 2>reports/err.$timestamp.txt
