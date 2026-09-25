#define _POSIX_C_SOURCE 200809L

#include "protocolo.h"
#include "jogo.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int fd;
    char nome[NOME_SIZE];
} Jogador;

typedef struct {
    Jogador j1;
    Jogador j2;
    int placar1;
    int placar2;
    int id_partida;
    unsigned int seed;
} Partida;

static int fd_escuta = -1;

static void tratar_sigint(int sig);
static void *thread_partida(void *arg);
static void executar_rodada(Partida *p, int num_rodada);

static void tratar_sigint(int sig) {
    (void)sig;
    printf("\n[*] Encerrando servidor...\n");
    if (fd_escuta != -1) {
        close(fd_escuta);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    int porta;
    int fd_espera = -1;
    char nome_espera[NOME_SIZE] = "";
    int proximo_id = 1;
    struct sockaddr_in endereco;
    int opt = 1;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, tratar_sigint);

    porta = (argc > 1) ? atoi(argv[1]) : PORTA_PADRAO;

    fd_escuta = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_escuta < 0) {
        perror("socket");
        exit(1);
    }

    setsockopt(fd_escuta, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&endereco, 0, sizeof(endereco));
    endereco.sin_family = AF_INET;
    endereco.sin_addr.s_addr = INADDR_ANY;
    endereco.sin_port = htons((unsigned short)porta);

    if (bind(fd_escuta, (struct sockaddr *)&endereco, sizeof(endereco)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(fd_escuta, 16) < 0) {
        perror("listen");
        exit(1);
    }

    printf("======================================\n");
    printf("   BATALHA DE PALAVRAS - Servidor\n");
    printf("   Porta: %d\n", porta);
    printf("   Aguardando jogadores (pares de 2)...\n");
    printf("======================================\n\n");

    for (;;) {
        struct sockaddr_in cliente_addr;
        socklen_t addr_len = sizeof(cliente_addr);
        int fd;
        int r;
        char linha[BUFFER_SIZE];
        char tipo[BUFFER_SIZE];
        char campos[4][BUFFER_SIZE];
        char nome_jogador[NOME_SIZE];

        fd = accept(fd_escuta, (struct sockaddr *)&cliente_addr, &addr_len);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        printf("[+] Jogador conectou: %s:%d (fd=%d)\n",
               inet_ntoa(cliente_addr.sin_addr), ntohs(cliente_addr.sin_port), fd);

        if (enviar_msg(fd, PROTO_NOME, "") < 0) {
            close(fd);
            continue;
        }

        if (receber_com_timeout(fd, 30) != 1) {
            printf("[!] Jogador (fd=%d) nao enviou nome a tempo, descartado\n", fd);
            close(fd);
            continue;
        }

        r = receber_linha(fd, linha, sizeof(linha));
        if (r <= 0) {
            printf("[!] Jogador (fd=%d) desconectou antes de enviar o nome\n", fd);
            close(fd);
            continue;
        }

        parse_mensagem(linha, tipo, sizeof(tipo), campos, 4);

        if (campos[0][0] != '\0') {
            strncpy(nome_jogador, campos[0], NOME_SIZE - 1);
            nome_jogador[NOME_SIZE - 1] = '\0';
        } else {
            snprintf(nome_jogador, NOME_SIZE, "Jogador%d", fd);
        }

        if (fd_espera == -1) {
            enviar_msg(fd, PROTO_AGUARDE, "Esperando outro jogador...");
            printf("[*] Aguardando mais 1 jogador(es)...\n");
            fd_espera = fd;
            strncpy(nome_espera, nome_jogador, NOME_SIZE - 1);
            nome_espera[NOME_SIZE - 1] = '\0';
        } else {
            Partida *p = malloc(sizeof(Partida));
            pthread_t tid;

            if (p == NULL) {
                fprintf(stderr, "Erro de memoria\n");
                close(fd);
                close(fd_espera);
                fd_espera = -1;
                continue;
            }

            p->j1.fd = fd_espera;
            strncpy(p->j1.nome, nome_espera, NOME_SIZE - 1);
            p->j1.nome[NOME_SIZE - 1] = '\0';

            p->j2.fd = fd;
            strncpy(p->j2.nome, nome_jogador, NOME_SIZE - 1);
            p->j2.nome[NOME_SIZE - 1] = '\0';

            p->placar1 = 0;
            p->placar2 = 0;
            p->id_partida = proximo_id++;
            p->seed = (unsigned int)time(NULL) ^ (unsigned int)getpid()
                      ^ ((unsigned int)p->id_partida * 2654435761u);

            printf("[Partida #%d] Jogadores: %s vs %s\n", p->id_partida, p->j1.nome, p->j2.nome);

            if (pthread_create(&tid, NULL, thread_partida, p) != 0) {
                perror("pthread_create");
                close(p->j1.fd);
                close(p->j2.fd);
                free(p);
            } else {
                pthread_detach(tid);
            }

            fd_espera = -1;
        }
    }

    return 0;
}

static void *thread_partida(void *arg) {
    Partida *p = (Partida *)arg;
    int rodada;
    char texto_fim[BUFFER_SIZE];

    enviar_msg(p->j1.fd, PROTO_MSG, "%s vs %s", p->j1.nome, p->j2.nome);
    enviar_msg(p->j2.fd, PROTO_MSG, "%s vs %s", p->j1.nome, p->j2.nome);

    for (rodada = 1; rodada <= TOTAL_RODADAS; rodada++) {
        executar_rodada(p, rodada);
    }

    if (p->placar1 > p->placar2) {
        snprintf(texto_fim, sizeof(texto_fim), "%s venceu! Placar final: %s %d x %d %s",
                 p->j1.nome, p->j1.nome, p->placar1, p->placar2, p->j2.nome);
    } else if (p->placar2 > p->placar1) {
        snprintf(texto_fim, sizeof(texto_fim), "%s venceu! Placar final: %s %d x %d %s",
                 p->j2.nome, p->j1.nome, p->placar1, p->placar2, p->j2.nome);
    } else {
        snprintf(texto_fim, sizeof(texto_fim), "Empate! Placar final: %s %d x %d %s",
                 p->j1.nome, p->placar1, p->placar2, p->j2.nome);
    }

    enviar_msg(p->j1.fd, PROTO_FIM, "%s", texto_fim);
    enviar_msg(p->j2.fd, PROTO_FIM, "%s", texto_fim);

    printf("[Partida #%d] %s\n", p->id_partida, texto_fim);

    close(p->j1.fd);
    close(p->j2.fd);
    free(p);

    return NULL;
}

static void executar_rodada(Partida *p, int num_rodada) {
    char letra = sortear_letra(&p->seed);
    time_t deadline;
    int pendente1 = 1;
    int pendente2 = 1;
    int recebido1 = 0;
    int recebido2 = 0;
    char resp1[BUFFER_SIZE] = "";
    char resp2[BUFFER_SIZE] = "";
    char palavra1[BUFFER_SIZE] = "";
    char palavra2[BUFFER_SIZE] = "";
    int valida1, valida2;
    char resultado1[2 * BUFFER_SIZE + NOME_SIZE + 64];
    char resultado2[2 * BUFFER_SIZE + NOME_SIZE + 64];

    enviar_msg(p->j1.fd, PROTO_RODADA, "%d|%c|%d", num_rodada, letra, TEMPO_LIMITE);
    enviar_msg(p->j2.fd, PROTO_RODADA, "%d|%c|%d", num_rodada, letra, TEMPO_LIMITE);

    printf("  [Rodada %d] Letra: %c\n", num_rodada, letra);

    deadline = time(NULL) + TEMPO_LIMITE + 2;

    while (pendente1 || pendente2) {
        fd_set conjunto;
        struct timeval tv;
        time_t agora = time(NULL);
        long restante = (long)(deadline - agora);
        int maxfd = 0;
        int r;

        if (restante <= 0) {
            break;
        }

        FD_ZERO(&conjunto);
        if (pendente1) {
            FD_SET(p->j1.fd, &conjunto);
            if (p->j1.fd > maxfd) {
                maxfd = p->j1.fd;
            }
        }
        if (pendente2) {
            FD_SET(p->j2.fd, &conjunto);
            if (p->j2.fd > maxfd) {
                maxfd = p->j2.fd;
            }
        }

        tv.tv_sec = restante;
        tv.tv_usec = 0;

        r = select(maxfd + 1, &conjunto, NULL, NULL, &tv);
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (r == 0) {
            break;
        }

        if (pendente1 && FD_ISSET(p->j1.fd, &conjunto)) {
            int rl = receber_linha(p->j1.fd, resp1, sizeof(resp1));
            if (rl <= 0) {
                printf("[!] Jogador %s desconectou\n", p->j1.nome);
                resp1[0] = '\0';
            } else {
                recebido1 = 1;
            }
            pendente1 = 0;
        }

        if (pendente2 && FD_ISSET(p->j2.fd, &conjunto)) {
            int rl = receber_linha(p->j2.fd, resp2, sizeof(resp2));
            if (rl <= 0) {
                printf("[!] Jogador %s desconectou\n", p->j2.nome);
                resp2[0] = '\0';
            } else {
                recebido2 = 1;
            }
            pendente2 = 0;
        }
    }

    if (recebido1) {
        char tipo[BUFFER_SIZE];
        char campos[4][BUFFER_SIZE];
        parse_mensagem(resp1, tipo, sizeof(tipo), campos, 4);
        if (strcmp(tipo, PROTO_PALAVRA) == 0) {
            strncpy(palavra1, campos[0], sizeof(palavra1) - 1);
            palavra1[sizeof(palavra1) - 1] = '\0';
        }
    }

    if (recebido2) {
        char tipo[BUFFER_SIZE];
        char campos[4][BUFFER_SIZE];
        parse_mensagem(resp2, tipo, sizeof(tipo), campos, 4);
        if (strcmp(tipo, PROTO_PALAVRA) == 0) {
            strncpy(palavra2, campos[0], sizeof(palavra2) - 1);
            palavra2[sizeof(palavra2) - 1] = '\0';
        }
    }

    valida1 = validar_palavra(palavra1, letra);
    valida2 = validar_palavra(palavra2, letra);

    if (valida1 && valida2 && strcasecmp(palavra1, palavra2) == 0) {
        valida1 = 0;
        valida2 = 0;
    }

    if (valida1) {
        p->placar1++;
    }
    if (valida2) {
        p->placar2++;
    }

    if (valida1) {
        snprintf(resultado1, sizeof(resultado1), "Palavra \"%s\" valida! +1 ponto. [%s enviou: \"%s\"]",
                 palavra1, p->j2.nome, palavra2);
    } else {
        snprintf(resultado1, sizeof(resultado1), "Palavra \"%s\" invalida. 0 pontos. [%s enviou: \"%s\"]",
                 palavra1, p->j2.nome, palavra2);
    }

    if (valida2) {
        snprintf(resultado2, sizeof(resultado2), "Palavra \"%s\" valida! +1 ponto. [%s enviou: \"%s\"]",
                 palavra2, p->j1.nome, palavra1);
    } else {
        snprintf(resultado2, sizeof(resultado2), "Palavra \"%s\" invalida. 0 pontos. [%s enviou: \"%s\"]",
                 palavra2, p->j1.nome, palavra1);
    }

    enviar_msg(p->j1.fd, PROTO_RESULTADO, "%s", resultado1);
    enviar_msg(p->j2.fd, PROTO_RESULTADO, "%s", resultado2);

    enviar_msg(p->j1.fd, PROTO_PLACAR, "%s|%d|%s|%d", p->j1.nome, p->placar1, p->j2.nome, p->placar2);
    enviar_msg(p->j2.fd, PROTO_PLACAR, "%s|%d|%s|%d", p->j1.nome, p->placar1, p->j2.nome, p->placar2);

    printf("  [Rodada %d] %s=\"%s\"(%s) | %s=\"%s\"(%s) | Placar: %d x %d\n",
           num_rodada, p->j1.nome, palavra1, valida1 ? "ok" : "falha",
           p->j2.nome, palavra2, valida2 ? "ok" : "falha", p->placar1, p->placar2);
}
