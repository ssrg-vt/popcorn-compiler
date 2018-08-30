#!/bin/bash

MIGTEST=3 make test-x86 && \
make transfer-checkpoint-to-arm && \
VERBOSE=1 RESUME=1 MIGTEST=3 make test-arm && \
make transfer-checkpoint-from-arm && \
RESUME=1 VERBOSE=1 MIGTEST=3 make test-x86 && \
make transfer-checkpoint-to-arm && \
VERBOSE=1 RESUME=1 MIGTEST=3 make test-arm && \
make transfer-checkpoint-from-arm && \
RESUME=1 VERBOSE=1 make test-x86
