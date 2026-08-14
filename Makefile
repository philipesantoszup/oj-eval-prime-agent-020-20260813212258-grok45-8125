.PHONY: all clean
all:
	gcc -O2 -w -Wno-error -o code main.c buddy.c

clean:
	rm -f code test
