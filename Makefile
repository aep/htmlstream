ALL: pretty tester

clean:
	rm -f test pretty

CFLAGS=-std=c99 -D_POSIX_C_SOURCE=1

tester: test.c
	$(CC) $(CFLAGS) $^ -o $@ -g

pretty: pretty.c
	$(CC) $(CFLAGS) $^ -o $@ -g


test: tester
	./tester
