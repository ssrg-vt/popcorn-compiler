# TODO: compile for aarch64 as well in this Makefile (same method as other
# libraries

POPCORN_INSTALL?=$(HOME)/hermit-popcorn/
CC=$(POPCORN_INSTALL)/x86_64-host/bin/clang

check-env: all
ifeq ($(ARCH), aarch64)
  AR=$(POPCORN_INSTALL)/x86_64-host/bin/aarch64-hermit-ar
  RANLIB=$(POPCORN_INSTALL)/x86_64-host/bin/aarch64-hermit-ranlib
  OUTPUT=libelf.a
  TARGET_LIB_DIR=$(POPCORN_INSTALL)/aarch64-hermit/lib/
  TARGET_HDR_DIR=$(POPCORN_INSTALL)/aarch64-hermit/include/libelf

  INCLUDES=-I. -isystem $(POPCORN_INSTALL)/aarch64-hermit/include \
		 -isystem $(POPCORN_INSTALL)/x86_64-host/lib/clang/3.7.1/include 

  CFLAGS=-target aarch64-hermit $(INCLUDES) -nostdlib -Wno-tautological-compare \
	   -O3 -ffunction-sections -fdata-sections -g -nostdinc -isystem .
else
  AR=$(POPCORN_INSTALL)/x86_64-host/bin/x86_64-hermit-ar
  RANLIB=$(POPCORN_INSTALL)/x86_64-host/bin/x86_64-hermit-ranlib
  OUTPUT=libelf.a
  TARGET_LIB_DIR=$(POPCORN_INSTALL)/x86_64-hermit/lib/
  TARGET_HDR_DIR=$(POPCORN_INSTALL)/x86_64-hermit/include/libelf

  INCLUDES=-I. -isystem $(POPCORN_INSTALL)/x86_64-hermit/include \
                 -isystem $(POPCORN_INSTALL)/x86_64-host/lib/clang/3.7.1/include

  CFLAGS=-target x86_64-hermit $(INCLUDES) -nostdlib -Wno-tautological-compare \
           -O3 -ffunction-sections -fdata-sections -g -nostdinc -isystem .  
endif

.PHONY: check-env 

SRCS = $(wildcard *.c)
HEADERS = $(wildcard *.h)
OBJS = $(SRCS:.c=.o)

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@echo " [AR-$(ARCH)] $@"
	@$(AR) rcv $@ $^ > /dev/null
	@echo " [RANLIB-$(ARCH)] $@"
	@$(RANLIB) $@ > /dev/null

%.o: %.c
	@echo " [CC-$(ARCH)] $<"
	@$(CC) -c $(CFLAGS) $< -o $@

install: $(OUTPUT) 
	@echo " [INSTALL] $(OUTPUT) to $(TARGET_LIB_DIR)"
	@mkdir -p $(TARGET_LIB_DIR)
	@cp -f $(OUTPUT) $(TARGET_LIB_DIR)
	@@echo " [INSTALL] $(HEADERS) to $(TARGET_LIB_DIR)"
	@mkdir -p $(TARGET_HDR_DIR)
	@cp -f $(HEADERS) $(TARGET_HDR_DIR) 
	rm -rf $(OBJS) $(OUTPUT)

clean:
	rm -rf $(OBJS) $(OUTPUT)

distclean: clean
