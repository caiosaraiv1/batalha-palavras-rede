CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -Iinclude

all: servidor cliente

servidor: src/servidor.c src/jogo.c include/jogo.h include/protocolo.h
	$(CC) $(CFLAGS) -o servidor src/servidor.c src/jogo.c -lpthread

cliente: src/cliente.c src/jogo.c include/jogo.h include/protocolo.h
	$(CC) $(CFLAGS) -o cliente src/cliente.c src/jogo.c

clean:
	rm -f servidor cliente

.PHONY: all clean
