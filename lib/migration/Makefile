POPCORN := /usr/local/popcorn
POPCORN_ARM := $(POPCORN)/aarch64
POPCORN_X86 := $(POPCORN)/x86_64

CC         := clang
CFLAGS     := -O3 -Wall -Werror -c -g -gdwarf-aranges -D_GNU_SOURCE \
              -ffunction-sections -fdata-sections -Wno-unused-variable
ifneq ($(findstring test,$(type)),)
CFLAGS     += -D_TEST_MIGRATE
endif
ifneq ($(findstring native,$(type)),)
CFLAGS     += -D_NATIVE
endif
CFLAGS_ARM := $(CFLAGS) -target aarch64-linux-gnu
CFLAGS_X86 := $(CFLAGS) -target x86_64-linux-gnu

INC     := -I./include
INC_ARM := $(INC) -I$(POPCORN_ARM)/include
INC_X86 := $(INC) -I$(POPCORN_X86)/include

OPT       := opt
OPT_FLAGS := -load $(POPCORN)/lib/libAssociateLiteral.so -associate-literal \
             -load $(POPCORN)/lib/libSectionStatic.so -section-static \
             -load $(POPCORN)/lib/libStackInfo.so -stack-info

AR := ar
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

LIB := libmigrate.a
LIB_ARM := $(BUILD_ARM)/$(LIB)
LIB_X86 := $(BUILD_X86)/$(LIB)

all: $(LIB_ARM) $(LIB_X86)

%/.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

$(BUILD_ARM)/%.o: src/%.c
	@echo " [CC] $< (aarch64)"
	@$(CC) $(CFLAGS_ARM) $(INC_ARM) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CC) $(CFLAGS_ARM) -o $@ $@.bc

$(LIB_ARM): $(BUILD_ARM)/.dir $(HDR_ARM) $(OBJ_ARM)
	@echo " [AR] $(LIB_ARM)"
	@rm -f $(LIB_ARM)
	@$(AR) $(ARFLAGS) $(LIB_ARM) $(OBJ_ARM) &> /dev/null

$(BUILD_X86)/%.o: src/%.c
	@echo " [CC] $< (x86-64)"
	@$(CC) $(CFLAGS_X86) $(INC_X86) -emit-llvm -o $@.bc $<
	@$(OPT) $(OPT_FLAGS) -o $@.bc $@.bc
	@$(CC) $(CFLAGS_X86) -o $@ $@.bc

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
