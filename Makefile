.PHONY: all clean
CC = gcc
CFLAGS = -O2 -std=gnu11 -Wno-int-conversion -Wno-unused-result

all:
	$(CC) $(CFLAGS) -o code main.c buddy.c

clean:
	rm -f code test
