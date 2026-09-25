#define _POSIX_C_SOURCE 200809L

#include "jogo.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int validar_palavra(const char *palavra, char letra) {
    size_t len, i;

    if (palavra == NULL) {
        return 0;
    }

    len = strlen(palavra);
    if (len < MIN_CARACTERES) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (!isalpha((unsigned char)palavra[i])) {
            return 0;
        }
    }

    if (toupper((unsigned char)palavra[0]) != toupper((unsigned char)letra)) {
        return 0;
    }

    return 1;
}

char sortear_letra(unsigned int *seed) {
    return (char)('A' + (rand_r(seed) % 26));
}

int enviar_msg(int fd, const char *tipo, const char *formato, ...) {
    char linha[BUFFER_SIZE + NOME_SIZE + 8];
    char corpo[BUFFER_SIZE];
    va_list args;

    corpo[0] = '\0';
    if (formato != NULL && formato[0] != '\0') {
        va_start(args, formato);
        vsnprintf(corpo, sizeof(corpo), formato, args);
        va_end(args);
    }

    snprintf(linha, sizeof(linha), "%s|%s\n", tipo, corpo);

    return (int)send(fd, linha, strlen(linha), 0);
}

int receber_linha(int fd, char *buffer, size_t tam) {
    size_t i = 0;
    char c;
    ssize_t r;

    if (tam == 0) {
        return -1;
    }

    while (i < tam - 1) {
        r = recv(fd, &c, 1, 0);
        if (r == 0) {
            return 0;
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (c == '\n') {
            break;
        }
        buffer[i++] = c;
    }

    buffer[i] = '\0';
    return (int)i;
}

int receber_com_timeout(int fd, int segundos) {
    fd_set conjunto;
    struct timeval tv;
    int r;

    FD_ZERO(&conjunto);
    FD_SET(fd, &conjunto);
    tv.tv_sec = segundos;
    tv.tv_usec = 0;

    r = select(fd + 1, &conjunto, NULL, NULL, &tv);
    if (r > 0) {
        return 1;
    }
    if (r == 0) {
        return 0;
    }
    return -1;
}

int parse_mensagem(char *linha, char *tipo, size_t tam_tipo,
                    char campos[][BUFFER_SIZE], int max_campos) {
    char *inicio = linha;
    char *barra;
    int n = 0;

    barra = strchr(inicio, '|');
    if (barra != NULL) {
        *barra = '\0';
    }
    strncpy(tipo, inicio, tam_tipo - 1);
    tipo[tam_tipo - 1] = '\0';

    if (barra == NULL) {
        return 0;
    }

    inicio = barra + 1;
    while (n < max_campos) {
        barra = strchr(inicio, '|');
        if (barra != NULL) {
            *barra = '\0';
        }
        strncpy(campos[n], inicio, BUFFER_SIZE - 1);
        campos[n][BUFFER_SIZE - 1] = '\0';
        n++;
        if (barra == NULL) {
            break;
        }
        inicio = barra + 1;
    }

    return n;
}
