CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-pthread

all: server deliver

server: server.o
	$(CC) server.o -o server $(LDFLAGS)

deliver: deliver.o
	$(CC) deliver.o -o deliver $(LDFLAGS)

clean:
	rm -f *.o server deliver