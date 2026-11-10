#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#include "resultado.h"

/* Lê uma linha do teclado com limite de tamanho. */
Resultado util_ler_linha(const char *mensagem, char *destino, size_t tamanho);
/* Lê um texto que não pode ficar vazio. */
Resultado util_ler_texto_obrigatorio(const char *mensagem, char *destino,
                                     size_t tamanho);
/* Lê um inteiro dentro de um intervalo. */
Resultado util_ler_inteiro(const char *mensagem, int minimo, int maximo,
                           int *valor);
/* Lê uma resposta de confirmação S ou N. */
Resultado util_ler_confirmacao(const char *mensagem, int *confirmado);
/* Remove espaços das extremidades do texto. */
void util_trim(char *texto);
/* Copia somente os dígitos para o vetor de destino. */
void util_somente_digitos(const char *origem, char *destino, size_t tamanho);
/* Verifica o formato e os dígitos de um documento. */
int util_validar_documento(const char *digitos);
/* Verifica o formato de um telefone. */
int util_validar_telefone(const char *digitos);
/* Verifica o formato básico de um e-mail. */
int util_validar_email(const char *email);
/* Devolve o ano atual do sistema. */
int util_ano_atual(void);
/* Aumenta a capacidade de um vetor alocado dinamicamente. */
void *util_dobrar_capacidade(void *vetor, size_t *capacidade,
                             size_t tamanho_item);
/* Imprime um texto alinhado em uma coluna. */
void util_imprimir_coluna(const char *texto, int largura);
/* Converte uma data do banco para exibição. */
void util_formatar_data(const char *iso, char *destino, size_t tamanho);
/* Imprime um título de tela. */
void util_titulo(const char *titulo);
/* Imprime uma mensagem de sucesso. */
void util_sucesso(const char *mensagem);
/* Imprime uma mensagem de erro. */
void util_erro(const char *mensagem);
/* Pausa a tela até o usuário pressionar Enter. */
void util_pausar(void);
/* Devolve o texto correspondente a um resultado. */
const char *util_texto_resultado(Resultado resultado);

#endif
