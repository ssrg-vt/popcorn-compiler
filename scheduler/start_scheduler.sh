#!/bin/bash

timestamp=$(date +%s)
mkdir -p reports
rm -fr /tmp/hermit-scheduler/
python -u ./scheduler.py "ep" 500 > reports/report.$timestamp.txt 2>reports/err.$timestamp.txt
