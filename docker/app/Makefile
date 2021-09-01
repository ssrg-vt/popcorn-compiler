CC	= /usr/local/popcorn/x86_64/bin/musl-clang
CFLAGS	= -static -popcorn-metadata -popcorn-target=x86_64-linux-gnu -secure-popcorn \
    -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -mno-red-zone
PREPARE	= /code/popcorn-chameleon/util/prepare-executable.sh

all:
	$(CC) $(CFLAGS) /code/app/test.c -o /code/app/test
	$(PREPARE) -f /code/app/test

clean:
	rm -rf test test.blacklist
