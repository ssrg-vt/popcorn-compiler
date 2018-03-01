POPCORN					:= /usr/local/popcorn
POPCORN_ARM			:= $(POPCORN)/aarch64
POPCORN_POWERPC	:= $(POPCORN)/powerpc64le
POPCORN_X86			:= $(POPCORN)/x86_64
SYSROOT					:= $(POPCORN_ARM)

CC			:= $(POPCORN)/bin/clang
CFLAGS	:= -Wall -g -nostdinc -D_MANUAL_PREFETCH -D_MANUAL_ASYNC -D_STATISTICS
ifeq ($(findstring debug,$(type)),)
CFLAGS	+= -O3 -DNDEBUG
else
CFLAGS	+= -O0 -D_DEBUG=1
endif

COMMON	:= $(shell readlink -f ../../common/include)
INC			:= -I./include -I$(COMMON) -isystem $(SYSROOT)/include

AR      := ar
ARFLAGS := -cq

HDR := $(shell ls include/*.h)
SRC := $(shell ls src/*.c)

BUILD         := build
BUILD_ARM			:= $(BUILD)/aarch64
BUILD_POWERPC	:= $(BUILD)/powerpc64le
BUILD_X86			:= $(BUILD)/x86_64

OBJ					:= $(subst src,$(BUILD),$(SRC:.c=.o))
OBJ_ARM			:= $(OBJ)
OBJ_POWERPC	:= $(OBJ:.o=_powerpc64le.o)
OBJ_X86			:= $(OBJ:.o=_x86_64.o)

LIB         := libdsm-prefetch.a
LIB_ARM     := $(BUILD_ARM)/$(LIB)
LIB_POWERPC := $(BUILD_POWERPC)/$(LIB)
LIB_X86     := $(BUILD_X86)/$(LIB)

TEST_SYSROOT	:= $(POPCORN)/x86_64
TEST_INC			:= $(subst $(SYSROOT),$(TEST_SYSROOT),$(INC))
TEST_CFLAGS		:= $(CFLAGS) $(TEST_INC) -nostdlib -pthread -L$(TEST_SYSROOT)/lib
TEST_LIBS			:= -lc -lmigrate -lstack-transform -lelf -lc
TEST_LDFLAGS	:= -L$(SYSROOT)/lib  $(TEST_SYSROOT)/lib/crt1.o $(TEST_LIBS)

TEST					:= test/prefetch-test
TEST_SRC			:= $(shell ls test/*.c)

# $(LIB_POWERPC)
all: $(LIB_ARM) $(LIB_X86)

%/.dir:
	@echo " [MKDIR] $*"
	@mkdir -p $*
	@touch $@

$(BUILD)/%.o: src/%.c
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -popcorn-migratable $(INC) -c -o $@ $<

$(OBJ_ARM): $(BUILD)/.dir $(HDR)
$(OBJ_POWERPC): $(OBJ_ARM)
$(OBJ_X86): $(OBJ_ARM)

$(LIB_ARM): $(BUILD_ARM)/.dir $(OBJ_ARM)
	@echo " [AR] $(LIB_ARM)"
	@rm -f $(LIB_ARM)
	@$(AR) $(ARFLAGS) $(LIB_ARM) $(OBJ_ARM) &> /dev/null

$(LIB_POWERPC): $(BUILD_POWERPC)/.dir $(OBJ_POWERPC)
	@echo " [AR] $(LIB_POWERPC)"
	@rm -f $(LIB_POWERPC)
	@$(AR) $(ARFLAGS) $(LIB_POWERPC) $(OBJ_POWERPC) &> /dev/null

$(LIB_X86): $(BUILD_X86)/.dir $(OBJ_X86)
	@echo " [AR] $(LIB_X86)"
	@rm -f $(LIB_X86)
	@$(AR) $(ARFLAGS) $(LIB_X86) $(OBJ_X86) &> /dev/null

# $(LIB_POWERPC)
#	@echo " [INSTALL] $(LIB_POWERPC) to $(POPCORN_POWERPC)/lib"
#	@cp $(LIB_POWERPC) $(POPCORN_POWERPC)/lib
#	@cp include/dsm-prefetch.h $(POPCORN_POWERPC)/include
install: $(LIB_ARM) $(LIB_X86)
	@echo " [INSTALL] $(LIB_ARM) to $(POPCORN_ARM)/lib"
	@cp $(LIB_ARM) $(POPCORN_ARM)/lib
	@cp include/dsm-prefetch.h $(POPCORN_ARM)/include
	@echo " [INSTALL] $(LIB_X86) to $(POPCORN_X86)/lib"
	@cp $(LIB_X86) $(POPCORN_X86)/lib
	@cp include/dsm-prefetch.h $(POPCORN_X86)/include

# Only test on x86
test: $(TEST_SRC) $(LIB_X86)
	@echo " [CC] $<"
	@$(CC) $(TEST_CFLAGS) -o $(TEST) $(TEST_SRC) $(LIB_X86) $(TEST_LDFLAGS)

clean:
	@echo " [RM] $(BUILD) $(TEST)"
	@rm -rf $(BUILD) $(TEST)

.PHONY: all install clean test
