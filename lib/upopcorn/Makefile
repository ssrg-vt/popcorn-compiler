POPCORN := /usr/local/popcorn
POPCORN_ARM := $(POPCORN)/aarch64
POPCORN_X86 := $(POPCORN)/x86_64

# Although we disable IR optimizations, use -O3 to get backend optimizations
CCARM         := $(POPCORN)/bin/clang
CCX86         := $(POPCORN)/bin/clang
#CCX86         := $(POPCORN)/bin/clang
CFLAGS     := -O3 -Xclang -disable-llvm-optzns -Wall -c -g -popcorn-alignment \
              -fno-vectorize -Wno-unused-variable -D_GNU_SOURCE -mllvm \
              -no-sm-warn
ifneq ($(findstring env_select,$(type)),)
CFLAGS     += -D_ENV_SELECT_MIGRATE
endif
ifneq ($(findstring native,$(type)),)
CFLAGS     += -D_NATIVE
endif
ifneq ($(findstring debug,$(type)),)
CFLAGS     += -D_DEBUG
endif
ifneq ($(findstring timing,$(type)),)
CFLAGS     += -D_TIME_REWRITE
endif
CFLAGS_ARM := $(CFLAGS) -target aarch64-linux-gnu
CFLAGS_X86 := $(CFLAGS) -target x86_64-linux-gnu

INC     := -I./include -I../stack_transformation/include/
INC_ARM := $(INC) -I$(POPCORN_ARM)/include
INC_X86 := $(INC) -I$(POPCORN_X86)/include

OPT       := $(POPCORN)/bin/opt
OPT_FLAGS := -O1 -disable-loop-vectorization -disable-slp-vectorization \
             -insert-stackmaps -no-live-vals

AR      := ar
ARFLAGS := -cq

HDR := $(shell ls include/*.h)
SRC := $(shell ls src/*.c)

BUILD := build
BUILD_ARM := $(BUILD)/aarch64
BUILD_X86 := $(BUILD)/x86_64

HDR_ARM := $(HDR) $(shell ls include/arch/aarch64/*.h) #$(subst include,include/arch/aarch64,$(HDR))
HDR_X86 := $(HDR) $(shell ls include/arch/x86_64/*.h) #$(subst include,include/arch/x86_64,$(HDR))

OBJ := $(SRC:.c=.o)
OBJ_ARM := $(subst src,$(BUILD_ARM),$(OBJ))
OBJ_X86 := $(subst src,$(BUILD_X86),$(OBJ))

LIB := libupopcorn.a
LIB_ARM := $(BUILD_ARM)/$(LIB)
LIB_X86 := $(BUILD_X86)/$(LIB)

all: $(LIB_ARM) $(LIB_X86)


%/.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

$(BUILD_ARM)/%.o: src/%.c $(HDR_ARM)
	export OBJDUMP=aarch64-linux-gnu-objdump
	export OBJCOPY=aarch64-linux-gnu-objcopy
	@echo " [CC] $< (aarch64)"
	#gcc -E $(INC_ARM) -o $@.cpp $<
	@$(CCARM) -E $(CFLAGS_ARM) $(INC_ARM) -o $@.cpp $<
	@$(CCARM) $(CFLAGS_ARM) $(INC_ARM) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CCARM) $(CFLAGS_ARM) -o $@ $@.bc

$(LIB_ARM): $(EXPORT_ARM) $(BUILD_ARM)/.dir $(HDR_ARM) $(OBJ_ARM)
	@echo " [AR] $(LIB_ARM)"
	@rm -f $(LIB_ARM)
	@$(AR) $(ARFLAGS) $(LIB_ARM) $(OBJ_ARM) &> /dev/null

$(BUILD_X86)/%.o: src/%.c $(HDR_X86)
	export OBJDUMP=objdump
	export OBJCOPY=objcopy
	@echo " [CC] $< (x86-64)"
	@$(CCX86) $(CFLAGS_X86) $(INC_X86) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CCX86) $(CFLAGS_X86) -o $@ $@.bc

$(LIB_X86): $(EXPORT_X86) $(BUILD_X86)/.dir $(HDR_X86) $(OBJ_X86)
	@echo " [AR] $(LIB_X86)"
	@rm -f $(LIB_X86)
	@$(AR) $(ARFLAGS) $(LIB_X86) $(OBJ_X86) &> /dev/null

install: $(LIB_ARM) $(LIB_X86)
	@echo " [INSTALL] $(LIB_ARM) to $(POPCORN_ARM)/lib"
	@cp $(LIB_ARM) $(POPCORN_ARM)/lib
	@cp $(HDR) $(POPCORN_ARM)/include
	@echo " [INSTALL] $(LIB_X86) to $(POPCORN_X86)/lib"
	@cp $(LIB_X86) $(POPCORN_X86)/lib
	@cp $(HDR) $(POPCORN_X86)/include

clean:
	@echo " [RM] $(BUILD)"
	@rm -rf $(BUILD)

.PHONY: all clean
