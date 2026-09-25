/*
 * jogo.h — Interface da lógica do jogo
 *
 * Validação de palavras, geração de letras e funções de
 * envio/recebimento formatadas pelo protocolo (protocolo.h).
 */

#ifndef JOGO_H
#define JOGO_H

#include <stddef.h>
#include "protocolo.h"

/* Retorna 1 se a palavra é válida (tamanho, charset, letra inicial), 0 caso contrário */
int validar_palavra(const char *palavra, char letra);

/* Sorteia uma letra maiúscula A-Z usando rand_r com a seed fornecida */
char sortear_letra(unsigned int *seed);

/* Monta "tipo|<formato>\n" e envia pelo socket fd; retorna o que send() retornou */
int enviar_msg(int fd, const char *tipo, const char *formato, ...);

/* Lê uma linha (terminada em '\n', que é removido) do fd para buffer de tam bytes.
 * Retorna bytes armazenados, 0 se a conexão foi fechada, -1 em erro. */
int receber_linha(int fd, char *buffer, size_t tam);

/* select() em fd aguardando até segundos; retorna 1 se há dado pronto, 0 se
 * estourou o tempo, -1 em erro. */
int receber_com_timeout(int fd, int segundos);

/* Separa linha "tipo|campo1|campo2|..." em tipo e até max_campos campos
 * (cada campos[i] com tamanho BUFFER_SIZE, campos vazios viram "").
 * Retorna a quantidade de campos preenchidos. */
int parse_mensagem(char *linha, char *tipo, size_t tam_tipo,
                    char campos[][BUFFER_SIZE], int max_campos);

#endif /* JOGO_H */
