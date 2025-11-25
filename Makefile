CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11

sheq4: sheq4.c
	$(CC) $(CFLAGS) -o sheq4 sheq4.c

test: sheq4
	./test.sh

clean:
	rm -f sheq4