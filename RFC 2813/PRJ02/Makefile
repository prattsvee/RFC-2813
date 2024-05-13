CC=gcc
CFLAGS=-w

all: client server

client: client.c
	$(CC) $(CFLAGS) client.c base64_utils.c -o client

server: server.c
	$(CC) $(CFLAGS) -o server server.c 



package:
	tar -czvf irc-package.tar.gz client server client.conf Makefile README.md

clean:
	rm -f client server