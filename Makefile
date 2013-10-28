all: test

test: mybuf.c test.c list.c
	$(CC) -Wextra -Werror -Wall -g -O0 -std=c89 -o $@ $^
