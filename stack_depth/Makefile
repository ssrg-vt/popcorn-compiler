POPCORN	:= /usr/local/popcorn

BIN		:= libstack-depth.a
BUILD	:= build

CXX				:= g++
CXXFLAGS	:= -O3 -Wall -std=c++11 

HDR	:= $(shell ls *.h)
SRC	:= $(shell ls *.cpp)
OBJ	:= $(addprefix $(BUILD)/,$(SRC:.cpp=.o))

all: $(BIN)

%/.dir:
	@mkdir -p $*
	@touch $@

build/%.o: %.cpp $(HDR)
	@echo " [CXX] $<"
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BIN): $(BUILD)/.dir $(OBJ)
	@rm -f $(BIN)
	@echo " [AR] $@"
	@ar -cq $(BIN) $(OBJ)

install: $(BIN)
	@echo " [INSTALL] $< to $(POPCORN)/lib"
	@cp $< $(POPCORN)/lib

clean:
	@echo " [CLEAN] $(BUILD) $(BIN)"
	@rm -rf $(BUILD) $(BIN)

.PHONY: all install clean
