CC = gcc
CFLAGS = -O2 -march=native -D_XOPEN_SOURCE=700 -xc -std=c11 -fstack-protector-all -Wall -Wextra -pedantic -g

all: new

new:
	$(CC) $(CFLAGS) -o bin/new_web_socket_server src/main.c -lcrypto -pthread

clean:
	$(RM) bin/*
