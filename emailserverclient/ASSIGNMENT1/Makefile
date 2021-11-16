CC = gcc
objects = emailserver emailclient

all: $(objects)

emailserver : emailserver.c
	$(CC) emailserver.c -o emailserver

emailclient : emailclient.c
	$(CC) emailclient.c -o emailclient

.PHONY: clean

clean: 
	rm $(objects)