cc = gcc -g
CC = g++ -g

all: multihash

multihash: src/multihash.cpp
	$(CC) -o bin/multihash src/multihash.cpp

clean:
	rm -f bin/*
