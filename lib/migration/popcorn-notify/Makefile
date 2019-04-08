FILE=popcorn-notify.c 
BIN=popcorn-notify
all: $(BIN)

$(BIN): $(FILE)
	gcc -g $< -o $@

install:
	install $(BIN) /usr/local/bin/$(BIN)

