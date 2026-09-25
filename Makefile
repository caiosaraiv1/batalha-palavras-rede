CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -Iinclude

all: bin/servidor bin/cliente

bin/servidor: src/servidor.c src/jogo.c include/jogo.h include/protocolo.h
	$(CC) $(CFLAGS) -o bin/servidor src/servidor.c src/jogo.c -lpthread

bin/cliente: src/cliente.c src/jogo.c include/jogo.h include/protocolo.h
	$(CC) $(CFLAGS) -o bin/cliente src/cliente.c src/jogo.c

clean:
	rm -f bin/servidor bin/cliente

.PHONY: all clean
