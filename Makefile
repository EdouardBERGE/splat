CC     ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2

all: splat

splat: splat.c
	$(CC) $(CFLAGS) -o $@ splat.c

tests/test_splat: tests/test_splat.c
	$(CC) $(CFLAGS) -o $@ tests/test_splat.c

test: splat tests/test_splat
	cd tests && ./test_splat ../splat

clean:
	rm -f splat splat.exe *.obj tests/test_splat tests/test_splat.exe tests/*.obj

.PHONY: all test clean
