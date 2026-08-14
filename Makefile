.PHONY: all clean
all:
	gcc -O2 -w -Wno-error -Wno-int-conversion -o code main.c buddy.c

clean:
	rm -f code test
