CC=gcc
CFLAGS=-O2 -Wall -Wextra -g
minishell: minishell.c
	$(CC) $(CFLAGS) -o $@ $<
test: minishell
	./test.sh
clean:
	rm -f minishell
