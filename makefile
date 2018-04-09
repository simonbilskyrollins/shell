CC=gcc

shell: shell.c
	$(CC) -o shell shell.c

clean:
	rm shell
