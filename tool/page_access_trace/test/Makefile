POPCORN := /usr/local/popcorn

BIN     := schedule-test

CC      := $(POPCORN)/x86_64/bin/musl-clang
CFLAGS  := -O2 -fopenmp=libiomp5 -distributed-omp -Wall -g
LIBS    := -lopenpop

$(BIN): schedule-test.c
	@echo " [CC] $<"
	@$(CC) $(CFLAGS) $< -o $@ $(LIBS)

clean:
	@echo " [RM] $(BIN)"
	@rm -f $(BIN)

.PHONY: clean
