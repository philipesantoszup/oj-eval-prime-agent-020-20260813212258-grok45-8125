.PHONY: all clean test
all:
	gcc -O2 -o code main.c buddy.c
	cp code test

clean:
	rm -f code test

test: all
	./code
