BIN	:= print-abort-counters
LIB	:= libabort-prof.a

CC 			:= clang
CXX 		:= clang++
CFLAGS	:= -O2 -Wall

SRC := src/abort-prof.c
OBJ	:= $(SRC:.c=.o)

AR			:= ar
ARFLAGS	:= -cq

all: $(LIB) $(BIN)
lib: $(LIB)
bin: $(BIN)

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

.PHONY: all lib bin clean
