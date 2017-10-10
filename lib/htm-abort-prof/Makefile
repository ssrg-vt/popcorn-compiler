POPCORN         := /usr/local/popcorn
POPCORN_X86_64  := $(POPCORN)/x86_64
POPCORN_PPC64   := $(POPCORN)/powerpc64

BIN	:= print-abort-counters
LIB	:= libabort-prof.a

# TODO PowerPC build/install

CC 			:= $(POPCORN)/bin/clang
CXX			:= $(POPCORN)/bin/clang++
CFLAGS	:= -O2 -Wall

SRC := src/abort-prof.c
OBJ	:= $(SRC:.c=.o)

AR			:= ar
ARFLAGS	:= -cq

all: $(LIB) $(BIN)
lib: $(LIB)
bin: $(BIN)
install: install-lib install-bin

install-lib: $(LIB)
	@echo " [INSTALL] $< to $(POPCORN_X86_64)/lib"
	@cp $< $(POPCORN_X86_64)/lib

install-bin: $(BIN)
	@echo " [INSTALL] $< to $(POPCORN)/bin"
	@cp $< $(POPCORN)/bin

src/%.o: src/%.c
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) -c -o $(<:.c=.o) $<

$(LIB): $(OBJ)
	@echo " [AR] $@"
	@$(AR) $(ARFLAGS) $@ $(OBJ) -o &> /dev/null

$(BIN): src/display.cpp
	@echo " [CXX] $<"
	@$(CXX) $(CFLAGS) -o $@ $<

clean:
	@echo " [RM] $(BIN) $(LIB) $(OBJ)"
	@rm -f $(BIN) $(LIB) $(OBJ)

.PHONY: all lib bin install install-lib install-bin clean
