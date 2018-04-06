POPCORN_INSTALL=/usr/local/popcorn/
HERMIT_PREFIX=/home/usr/hermit/

CC=$(POPCORN_INSTALL)/bin/clang
AR=$(HERMIT_PREFIX)/bin/x86_64-hermit-ar
RANLIB=$(HERMIT_PREFIX)/bin/x86_64-hermit-ranlib
OUTPUT=libelf.a
TARGET_LIB_DIR=$(HERMIT_PREFIX)/x86_64-hermit/lib/
TARGET_HDR_DIR=$(HERMIT_PREFIX)/x86_64-hermit/include/libelf

CFLAGS=-I. -target x86_64-hermit -nostdlib -isystem $(HERMIT_PREFIX)/x86_64-hermit/include -isystem /usr/local/popcorn/bin/../lib/clang/3.7.1/include -Wno-tautological-compare -O0 -g -nostdinc -isystem .

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
