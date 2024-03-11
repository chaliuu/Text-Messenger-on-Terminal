CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-pthread

all: server client

server: server.o
	$(CC) server.o -o server $(LDFLAGS)

deliver: client.o
	$(CC) client.o -o client $(LDFLAGS)

clean:
	rm -f *.o server client