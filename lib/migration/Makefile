POPCORN         := /home/elfmaster/build.4-23
POPCORN_ARM     := $(POPCORN)/aarch64
POPCORN_POWERPC := $(POPCORN)/powerpc64
POPCORN_X86     := $(POPCORN)/x86_64

# Although we disable IR optimizations, use -O3 to get backend optimizations
CC         := $(POPCORN)/bin/clang
CFLAGS     := -O0 -mllvm -optimize-regalloc -Wall -fPIC -c -g -popcorn-alignment \
              -Wno-unused-variable -D_GNU_SOURCE -mllvm -no-sm-warn
OPT_LEVEL  := -O3
ifneq ($(findstring env_select,$(type)),)
CFLAGS     += -D_ENV_SELECT_MIGRATE=1
endif
ifneq ($(findstring native,$(type)),)
CFLAGS     += -D_NATIVE=1
endif
ifneq ($(findstring debug,$(type)),)
CFLAGS     += -D_DEBUG=1
OPT_LEVEL  :=
endif
ifneq ($(findstring nocleancrash,$(type)),)
CFLAGS     += -D_CLEAN_CRASH=0
endif
ifneq ($(findstring timing,$(type)),)
CFLAGS     += -D_TIME_REWRITE=1 -D_TIME_RESPONSE_DELAY=1
endif
ifneq ($(findstring signal_trigger,$(type)),)
CFLAGS     += -D_SIG_MIGRATION=1
endif
CFLAGS_ARM     := $(CFLAGS) -target aarch64-linux-gnu
CFLAGS_POWERPC := $(CFLAGS) -target powerpc64le-linux-gnu
CFLAGS_X86     := $(CFLAGS) -target x86_64-linux-gnu

COMMON      := $(shell readlink -f ../../common/include)
INC         := -I./include -I$(COMMON)
INC_ARM     := $(INC) -I$(POPCORN_ARM)/include
INC_POWERPC := $(INC) -I$(POPCORN_POWERPC)/include
INC_X86     := $(INC) -I$(POPCORN_X86)/include

OPT       := $(POPCORN)/bin/opt
OPT_FLAGS := $(OPT_LEVEL) \
             -disable-slp-vectorization -insert-stackmaps \
             -no-live-vals

ifeq ($(LLVM_VERSION), 3.7)
OPT_FLAGS += -disable-loop-vectorization
endif

AS_ARM           := aarch64-linux-gnu-as
AS_POWERPC       := powerpc64le-linux-gnu-as
AS_X86           := x86_64-linux-gnu-as
AS_FLAGS_ARM     := --defsym __aarch64__=1
AS_FLAGS_POWERPC := --defsym __powerpc64__=1
AS_FLAGS_X86     := --defsym __x86_64__=1

AR      := ar
ARFLAGS := -cq

HDR := $(shell ls include/*.h)
SRC := $(shell ls src/*.c)
ASM := $(shell find -iname "migrate_*.s")

BUILD         := build
BUILD_ARM     := $(BUILD)/aarch64
BUILD_POWERPC := $(BUILD)/powerpc64
BUILD_X86     := $(BUILD)/x86_64

HDR_ARM     := $(HDR) $(shell ls include/arch/aarch64/*.h)
HDR_POWERPC := $(HDR) $(shell ls include/arch/powerpc64/*.h)
HDR_X86     := $(HDR) $(shell ls include/arch/x86_64/*.h)

OBJ         := $(SRC:.c=.o) $(ASM:.s=.o)
OBJ_ARM     := $(subst src,$(BUILD_ARM),$(OBJ))
OBJ_POWERPC := $(subst src,$(BUILD_POWERPC),$(OBJ))
OBJ_X86     := $(subst src,$(BUILD_X86),$(OBJ))

LIB         := libmigrate.a
LIB_ARM     := $(BUILD_ARM)/$(LIB)
LIB_POWERPC := $(BUILD_POWERPC)/$(LIB)
LIB_X86     := $(BUILD_X86)/$(LIB)

# $(LIB_POWERPC)
all: $(LIB_ARM) $(LIB_X86)

%/.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

$(BUILD_POWERPC)/%.o: src/%.c $(HDR_POWERPC)
	@echo " [CC] $< (powerpc64)"
	@$(CC) $(CFLAGS_POWERPC) $(INC_POWERPC) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CC) $(CFLAGS_POWERPC) -o $@ $@.bc

$(BUILD_POWERPC)/arch/aarch64/%.o: src/arch/aarch64/%.s
	@echo " [AS] $< (powerpc64)"
	@$(AS_POWERPC) $(AS_FLAGS_POWERPC) -o $@ $<

$(BUILD_POWERPC)/arch/powerpc64/%.o: src/arch/powerpc64/%.s
	@echo " [AS] $< (powerpc64)"
	@$(AS_POWERPC) $(AS_FLAGS_POWERPC) -o $@ $<

$(BUILD_POWERPC)/arch/x86_64/%.o: src/arch/x86_64/%.s
	@echo " [AS] $< (powerpc64)"
	@$(AS_POWERPC) $(AS_FLAGS_POWERPC) -o $@ $<

$(OBJ_POWERPC): $(BUILD_POWERPC)/.dir $(BUILD_POWERPC)/arch/aarch64/.dir \
		 $(BUILD_POWERPC)/arch/powerpc64/.dir \
		 $(BUILD_POWERPC)/arch/x86_64/.dir
$(LIB_POWERPC): $(HDR_POWERPC) $(OBJ_POWERPC)
	@echo " [AR] $(LIB_POWERPC)"
	@rm -f $(LIB_POWERPC)
	@$(AR) $(ARFLAGS) $(LIB_POWERPC) $(OBJ_POWERPC) &> /dev/null

$(BUILD_ARM)/%.o: src/%.c $(HDR_ARM)
	@echo " [CC] $< (aarch64)"
	@$(CC) $(CFLAGS_ARM) $(INC_ARM) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CC) $(CFLAGS_ARM) -o $@ $@.bc

$(BUILD_ARM)/arch/aarch64/%.o: src/arch/aarch64/%.s
	@echo " [AS] $< (aarch64)"
	@$(AS_ARM) $(AS_FLAGS_ARM) -o $@ $<

$(BUILD_ARM)/arch/powerpc64/%.o: src/arch/powerpc64/%.s
	@echo " [AS] $< (aarch64)"
	@$(AS_ARM) $(AS_FLAGS_ARM) -o $@ $<

$(BUILD_ARM)/arch/x86_64/%.o: src/arch/x86_64/%.s
	@echo " [AS] $< (aarch64)"
	@$(AS_ARM) $(AS_FLAGS_ARM) -o $@ $<

$(OBJ_ARM): $(BUILD_ARM)/.dir $(BUILD_ARM)/arch/aarch64/.dir \
	     $(BUILD_ARM)/arch/powerpc64/.dir $(BUILD_ARM)/arch/x86_64/.dir
$(LIB_ARM): $(HDR_ARM) $(OBJ_ARM)
	@echo " [AR] $(LIB_ARM)"
	@rm -f $(LIB_ARM)
	@$(AR) $(ARFLAGS) $(LIB_ARM) $(OBJ_ARM) &> /dev/null

$(BUILD_X86)/%.o: src/%.c $(HDR_X86)
	@echo " [CC] $< (x86-64)"
	@$(CC) $(CFLAGS_X86) $(INC_X86) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CC) $(CFLAGS_X86) -o $@ $@.bc

$(BUILD_X86)/arch/aarch64/%.o: src/arch/aarch64/%.s $(BUILD_X86)/arch/aarch64/.dir
	@echo " [AS] $< (x86-64)"
	@$(AS_X86) $(AS_FLAGS_X86) -o $@ $<

$(BUILD_X86)/arch/powerpc64/%.o: src/arch/powerpc64/%.s $(BUILD_X86)/arch/powerpc64/.dir
	@echo " [AS] $< (x86-64)"
	@$(AS_X86) $(AS_FLAGS_X86) -o $@ $<

$(BUILD_X86)/arch/x86_64/%.o: src/arch/x86_64/%.s $(BUILD_X86)/arch/x86_64/.dir
	@echo " [AS] $< (x86-64)"
	@$(AS_X86) $(AS_FLAGS_X86) -o $@ $<

$(OBJ_X86): $(BUILD_X86)/.dir $(BUILD_X86)/arch/aarch64/.dir \
	     $(BUILD_X86)/arch/powerpc64/.dir $(BUILD_X86)/arch/x86_64/.dir
$(LIB_X86): $(HDR_X86) $(OBJ_X86)
	@echo " [AR] $(LIB_X86)"
	@rm -f $(LIB_X86)
	@$(AR) $(ARFLAGS) $(LIB_X86) $(OBJ_X86) &> /dev/null

# $(LIB_POWERPC)
#	@echo " [INSTALL] $(LIB_POWERPC) to $(POPCORN_POWERPC)/lib"
#	@cp $(LIB_POWERPC) $(POPCORN_POWERPC)/lib
#	@cp include/migrate.h $(POPCORN_POWERPC)/include
install: $(LIB_ARM) $(LIB_X86)
	@echo " [INSTALL] $(LIB_ARM) to $(POPCORN_ARM)/lib"
	@cp $(LIB_ARM) $(POPCORN_ARM)/lib
	@cp include/migrate.h $(POPCORN_ARM)/include
	@echo " [INSTALL] $(LIB_X86) to $(POPCORN_X86)/lib"
	@cp $(LIB_X86) $(POPCORN_X86)/lib
	@cp include/migrate.h $(POPCORN_X86)/include

clean:
	@echo " [RM] $(BUILD)"
	@rm -rf $(BUILD)

.PHONY: all install clean
