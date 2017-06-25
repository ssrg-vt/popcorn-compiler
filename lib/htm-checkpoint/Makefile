POPCORN     := /usr/local/popcorn
POPCORN_PPC := $(POPCORN)/ppc64le
POPCORN_X86 := $(POPCORN)/x86_64

CC := gcc #$(POPCORN)/bin/clang++

INC     := -I./include
INC_PPC := $(INC) -I./arch/ppc64le
INC_X86 := $(INC) -I./arch/x86_64

ARCH_PPC := -mtune=power8 -mhtm #-target ppc64le-linux-gnu -Xclang -target-cpu -Xclang pwr8
ARCH_X86 := -mtune=broadwell -mrtm #-target x86_64-linux-gnu -Xclang -target-cpu -Xclang broadwell

CFLAGS     := -O2 -Wall -c -g
CFLAGS_PPC := $(CFLAGS) $(ARCH_PPC) $(INC_PPC)
CFLAGS_X86 := $(CFLAGS) $(ARCH_X86) $(INC_X86)

AR      := ar
ARFLAGS := -cq

HDR     := $(shell ls include/*.h)
HDR_PPC := $(HDR) $(shell ls arch/ppc64le/*.h)
HDR_X86 := $(HDR) $(shell ls arch/x86_64/*.h)

SRC     := $(shell ls src/*.c)

BUILD     := build
BUILD_PPC := $(BUILD)/ppc64le
BUILD_X86 := $(BUILD)/x86_64

OBJ     := $(SRC:.c=.o)
OBJ_PPC := $(subst src,$(BUILD_PPC),$(OBJ))
OBJ_X86 := $(subst src,$(BUILD_X86),$(OBJ))

LIB     := libhtm-checkpoint.a
LIB_PPC := $(BUILD_PPC)/$(LIB)
LIB_X86 := $(BUILD_X86)/$(LIB)

OBJ_STATS     := $(SRC:.c=_stats.o)
OBJ_STATS_PPC := $(subst src,$(BUILD_PPC),$(OBJ_STATS))
OBJ_STATS_X86 := $(subst src,$(BUILD_X86),$(OBJ_STATS))

LIB_STATS     := $(LIB:.a=-stats.a)
LIB_STATS_PPC := $(BUILD_PPC)/$(LIB_STATS)
LIB_STATS_X86 := $(BUILD_X86)/$(LIB_STATS)

all: $(LIB_PPC) $(LIB_X86)

%/.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

###########
# PowerPC #
###########

$(BUILD_PPC)/%.o: src/%.c $(HDR_PPC)
	@echo " [CC] $< (ppc64le)"
	@$(CC) $(CFLAGS_PPC) -o $@ $<

$(LIB_PPC): $(BUILD_PPC)/.dir $(OBJ_PPC)
	@echo " [AR] $@"
	@rm -f $@
	@$(AR) $(ARFLAGS) $@ $(OBJ_PPC) &> /dev/null

$(BUILD_PPC)/%_stats.o: src/%.c $(HDR_PPC)
	@echo " [CC] $< (ppc64le, statistics enabled)"
	@$(CC) $(CFLAGS_PPC) -D_STATISTICS -o $@ $<

$(LIB_STATS_PPC): $(BUILD_PPC)/.dir $(OBJ_STATS_PPC)
	@echo " [AR] $@"
	@rm -f $@
	@$(AR) $(ARFLAGS) $@ $(OBJ_STATS_PPC) &> /dev/null

##########
# x86-64 #
##########

$(BUILD_X86)/%.o: src/%.c $(HDR_X86)
	@echo " [CC] $< (x86-64)"
	@$(CC) $(CFLAGS_X86) -o $@ $<

$(LIB_X86): $(BUILD_X86)/.dir $(OBJ_X86)
	@echo " [AR] $@"
	@rm -f $@
	@$(AR) $(ARFLAGS) $@ $(OBJ_X86) &> /dev/null

$(BUILD_X86)/%_stats.o: src/%.c $(HDR_X86)
	@echo " [CC] $< (x86-64, statistics enabled)"
	@$(CC) $(CFLAGS_X86) -D_STATISTICS -o $@ $<

$(LIB_STATS_X86): $(BUILD_X86)/.dir $(OBJ_STATS_X86)
	@echo " [AR] $@"
	@rm -f $@
	@$(AR) $(ARFLAGS) $@ $(OBJ_STATS_X86) &> /dev/null

install: $(LIB_PPC) $(LIB_X86)
	@echo " [INSTALL] $(LIB_PPC) to $(POPCORN_PPC)/lib"
	@cp $(LIB_PPC) $(POPCORN_PPC)/lib
	@cp $(HDR) $(POPCORN_PPC)/include
	@echo " [INSTALL] $(LIB_X86) to $(POPCORN_X86)/lib"
	@cp $(LIB_X86) $(POPCORN_X86)/lib
	@cp $(HDR) $(POPCORN_X86)/include

clean:
	@echo " [RM] $(BUILD)"
	@rm -rf $(BUILD)

.PHONY: all install clean
