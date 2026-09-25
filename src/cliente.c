#define _POSIX_C_SOURCE 200809L

#include "protocolo.h"
#include "jogo.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

static int fd_socket = -1;

static void tratar_sigint(int sig) {
    (void)sig;
    printf("\n[*] Encerrando cliente...\n");
    if (fd_socket != -1) {
        close(fd_socket);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    const char *ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int porta = (argc > 2) ? atoi(argv[2]) : PORTA_PADRAO;
    struct sockaddr_in endereco;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, tratar_sigint);

    printf("========================================\n");
    printf("     BATALHA DE PALAVRAS - Cliente\n");
    printf("========================================\n");
    printf("  Conectando a %s:%d...\n", ip, porta);

    fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket < 0) {
        perror("socket");
        exit(1);
    }

    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_port = htons((unsigned short)porta);
    if (inet_pton(AF_INET, ip, &endereco.sin_addr) <= 0) {
        fprintf(stderr, "Endereco invalido: %s\n", ip);
        exit(1);
    }

    if (connect(fd_socket, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("connect");
        exit(1);
    }

    printf("  Conectado!\n\n");

    for (;;) {
        char linha[BUFFER_SIZE];
        char tipo[BUFFER_SIZE];
        char campos[4][BUFFER_SIZE];
        int r;

        r = receber_linha(fd_socket, linha, sizeof(linha));
        if (r <= 0) {
            printf("[!] Conexao com o servidor encerrada.\n");
            break;
        }

        parse_mensagem(linha, tipo, sizeof(tipo), campos, 4);

        if (strcmp(tipo, PROTO_NOME) == 0) {
            char nome[NOME_SIZE];

            printf("  Digite seu nome: ");
            fflush(stdout);
            if (fgets(nome, sizeof(nome), stdin) == NULL) {
                nome[0] = '\0';
            }
            nome[strcspn(nome, "\n")] = '\0';
            enviar_msg(fd_socket, PROTO_NOME, "%s", nome);
            printf("  Bem-vindo, %s!\n\n", nome);
        } else if (strcmp(tipo, PROTO_AGUARDE) == 0) {
            printf("   %s\n\n", campos[0]);
        } else if (strcmp(tipo, PROTO_MSG) == 0) {
            printf("   %s\n\n", campos[0]);
        } else if (strcmp(tipo, PROTO_RODADA) == 0) {
            int num = atoi(campos[0]);
            char letra = campos[1][0];
            int tempo = atoi(campos[2]);
            char palavra[BUFFER_SIZE];
            int pronto;

            printf("  ================================\n");
            printf("       RODADA %d de %d\n", num, TOTAL_RODADAS);
            printf("    Letra: [%c]   Tempo: %d seg\n", letra, tempo);
            printf("    Minimo: %d caracteres\n", MIN_CARACTERES);
            printf("  ================================\n");
            printf("  Sua palavra: ");
            fflush(stdout);

            pronto = receber_com_timeout(STDIN_FILENO, tempo);
            if (pronto == 1) {
                ssize_t n = read(STDIN_FILENO, palavra, sizeof(palavra) - 1);
                if (n <= 0) {
                    printf("  Erro ao ler entrada.\n");
                    enviar_msg(fd_socket, PROTO_TIMEOUT, "");
                } else {
                    palavra[n] = '\0';
                    palavra[strcspn(palavra, "\n")] = '\0';
                    enviar_msg(fd_socket, PROTO_PALAVRA, "%s", palavra);
                    printf("  Enviado: \"%s\" - aguardando resultado...\n", palavra);
                }
            } else if (pronto == 0) {
                tcflush(STDIN_FILENO, TCIFLUSH);
                printf("  Tempo esgotado!\n");
                enviar_msg(fd_socket, PROTO_TIMEOUT, "");
            } else {
                printf("  Erro ao aguardar entrada.\n");
                enviar_msg(fd_socket, PROTO_TIMEOUT, "");
            }
        } else if (strcmp(tipo, PROTO_RESULTADO) == 0) {
            printf("   %s\n", campos[0]);
        } else if (strcmp(tipo, PROTO_PLACAR) == 0) {
            printf("  --------------------------------\n");
            printf("  PLACAR: %s %s  x  %s %s\n", campos[0], campos[1], campos[2], campos[3]);
            printf("  --------------------------------\n\n");
        } else if (strcmp(tipo, PROTO_FIM) == 0) {
            printf("\n  %s\n", campos[0]);
            break;
        }
    }

    close(fd_socket);
    return 0;
}
