#!/bin/bash

timestamp=$(date +%s)
mkdir -p reports
rm -r /tmp/test/
python -u ./scheduler.py "ep" 900 > reports/report.$timestamp.txt
