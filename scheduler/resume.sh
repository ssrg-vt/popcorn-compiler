#!/bin/bash
set -x

rsync --no-whole-file /home/karaoui/hermit-popcorn/aarch64-hermit/bin/proxy potato:/home/libre/
ssh potato "HERMIT_ISLE=uhyve HERMIT_MEM=2G HERMIT_CPUS=1 
        HERMIT_VERBOSE=0 HERMIT_MIGTEST=0 
        HERMIT_MIGRATE_RESUME=1 HERMIT_DEBUG=0 
        HERMIT_NODE_ID=1 ST_AARCH64_BIN=prog_aarch64_aligned 
        ST_X86_64_BIN=prog_x86-64_aligned cd $1; 
        /home/libre/proxy ./prog_aarch64_aligned"
