CC=gcc

all: shm-server shm-client
.PHONY: all

shm-server:
	$(CC) -o server server-main.c;

shm-client:
	$(CC) -o client client-main.c;
