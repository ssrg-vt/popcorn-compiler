POPCORN := /usr/local/popcorn
POPCORN_ARM := $(POPCORN)/aarch64
POPCORN_X86 := $(POPCORN)/x86_64

# Although we disable IR optimizations, use -O3 to get backend optimizations
CC         := $(POPCORN)/bin/clang
CFLAGS     := -O0 -nostdinc -Xclang -disable-llvm-optzns -Wall -c -g -popcorn-migratable \
              -fno-vectorize -Wno-unused-variable -D_GNU_SOURCE -mllvm \
              -no-sm-warn -fno-common -ftls-model=initial-exec

ifneq ($(findstring debug,$(type)),)
CFLAGS     += -D_DEBUG
endif
CFLAGS_ARM := $(CFLAGS) 
CFLAGS_X86 := $(CFLAGS)

INC     := -I./include
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

HDR_ARM := $(HDR) $(subst include,include/arch/aarch64,$(HDR))
HDR_X86 := $(HDR) $(subst include,include/arch/x86_64,$(HDR))

OBJ := $(SRC:.c=.o)
OBJ_ARM := $(subst src,$(BUILD_ARM),$(OBJ))
OBJ_X86 := $(subst src,$(BUILD_X86),$(OBJ))

LIB := liboffload.a
LIB_ARM := $(BUILD_ARM)/$(LIB)
LIB_X86 := $(BUILD_X86)/$(LIB)

all: $(LIB_ARM) $(LIB_X86)

%/.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

$(BUILD_ARM)/%.o: src/%.c $(HDR_ARM)
	@echo " [CC] $< (aarch64)"
	@$(CC) $(CFLAGS_ARM) $(INC_ARM) -o $@ $<

$(LIB_ARM): $(BUILD_ARM)/.dir $(HDR_ARM) $(OBJ_ARM)
	@echo " [AR] $(LIB_ARM)"
	@rm -f $(LIB_ARM)
	@$(AR) $(ARFLAGS) $(LIB_ARM) $(OBJ_ARM) &> /dev/null

$(BUILD_X86)/%.o: src/%.c $(HDR_X86)
	@echo " [CC] $< (x86-64)"
	@$(CC) $(CFLAGS_X86) $(INC_X86) -o $@ $<
	@#what an ugly hack :)
	@cp $(BUILD_X86)/*x86_64.o $@
	@rm $(BUILD_X86)/*x86_64.o

$(LIB_X86): $(BUILD_X86)/.dir $(HDR_X86) $(OBJ_X86)
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
