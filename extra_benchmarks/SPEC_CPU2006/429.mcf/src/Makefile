BIN     := 429_mcf
C_SRC 	:= $(shell ls *.c)
APP_DIR := $(shell pwd)

# TODO -funroll-loops -fprefetch-loop-arrays
CFLAGS	:= -fpermissive -fno-exceptions -DENABLE_OPENMP -DERR_CHK

include ../../Makefile

