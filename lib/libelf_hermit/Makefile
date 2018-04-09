# TODO: compile for aarch64 as well in this Makefile (same method as other
# libraries

POPCORN_INSTALL?=~/hermit-popcorn/
HERMIT_PREFIX=/home/usr/hermit/ #TODO
INSTALL_PREFIX?=$(HERMIT_PREFIX)

CC=$(POPCORN_INSTALL)/x86_64-host/bin/clang
AR=$(HERMIT_PREFIX)/bin/x86_64-hermit-ar
RANLIB=$(HERMIT_PREFIX)/bin/x86_64-hermit-ranlib
OUTPUT=libelf.a
TARGET_LIB_DIR=$(INSTALL_PREFIX)/x86_64-hermit/lib/
TARGET_HDR_DIR=$(INSTALL_PREFIX)/x86_64-hermit/include/libelf

INCLUDES=-I. -isystem $(HERMIT_PREFIX)/x86_64-hermit/include \
		 -isystem $(POPCORN_INSTALL)/x86_64-host/lib/clang/3.7.1/include 

CFLAGS=-target x86_64-hermit $(INCLUDES) -nostdlib -Wno-tautological-compare \
	   -O3 -ffunction-sections -fdata-sections -g -nostdinc -isystem .

SRCS = $(wildcard *.c)
HEADERS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@echo " [AR-X86_64] $@"
	@$(AR) rcv $@ $^ > /dev/null
	@echo " [RANLIB-X86_64] $@"
	@$(RANLIB) $@ > /dev/null

%.o: %.c
	@echo " [CC-X86_64] $<"
	@$(CC) -c $(CFLAGS) $< -o $@

install: $(OUTPUT)
	@echo " [INSTALL] $(OUTPUT) to $(TARGET_LIB_DIR)"
	@mkdir -p $(TARGET_LIB_DIR)
	@cp -f $(OUTPUT) $(TARGET_LIB_DIR)
	@@echo " [INSTALL] $(HEADERS) to $(TARGET_LIB_DIR)"
	@mkdir -p $(TARGET_HDR_DIR)
	@cp -f $(HEADERS) $(TARGET_HDR_DIR) 

clean:
	rm -rf $(OBJS) $(OUTPUT)

distclean: clean
