CC = gcc
CFLAGS = -g -O0 -std=c11 -static -Wall -Isrc -rdynamic -fomit-frame-pointer -Wl,--whole-archive -lpthread -Wl,--no-whole-archive

all: $(patsubst %.c, %.out, $(wildcard *.c))

%.out: %.c
	$(CC) $(CFLAGS) $< -o $(@:.out=)

clean: $(patsubst %.c, %.clean, $(wildcard *.c))

%.clean:
	rm -f $(@:.clean=)