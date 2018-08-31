#!/bin/bash

timestamp=$(date +%s)
rm -r /tmp/test/
python -u ./scheduler.py "ep"  800 > report.$timestamp.txt
