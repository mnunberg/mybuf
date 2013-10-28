all: test

test: mybuf.c test.c
	$(CC) -Wall -std=c89 -o $@ $^
