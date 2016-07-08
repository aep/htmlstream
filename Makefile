ALL: test pretty

test: test.c
	$(CC) $^ -o $@ -g
pretty: pretty.c
	$(CC) $^ -o $@ -g
