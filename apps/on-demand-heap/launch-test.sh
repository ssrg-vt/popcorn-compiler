#!/bin/bash

# Simple script that starts the program on the local machine, migrate to the
# TARGET after MIG_AFTER seconds, then migrate back after an additional
# MIG_AFTER seconds to finish execution on the local machine

# IP of the remote machine:
TARGET=10.2.2.136
# NFS mount on the remote machine
TARGET_NFS_MOUNT=/mnt/nfs/var/nfs
# Amount of time in sec before first migration, and between first and second
MIG_AFTER=90

dir=${PWD##*/}
MIGTEST=$MIG_AFTER make test && ssh $TARGET "RESUME=1 MIGTEST=$MIG_AFTER make -C $TARGET_NFS_MOUNT/$dir test" && RESUME=1 make test
