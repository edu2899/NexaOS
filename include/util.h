#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#include "resultado.h"

Resultado util_ler_linha(const char *mensagem, char *destino, size_t tamanho);
Resultado util_ler_texto_obrigatorio(const char *mensagem, char *destino,
                                     size_t tamanho);
Resultado util_ler_inteiro(const char *mensagem, int minimo, int maximo,
                           int *valor);
Resultado util_ler_confirmacao(const char *mensagem, int *confirmado);
void util_trim(char *texto);
void util_somente_digitos(const char *origem, char *destino, size_t tamanho);
int util_validar_documento(const char *digitos);
int util_validar_telefone(const char *digitos);
int util_validar_email(const char *email);
int util_ano_atual(void);
void *util_dobrar_capacidade(void *vetor, size_t *capacidade,
                             size_t tamanho_item);
void util_imprimir_coluna(const char *texto, int largura);
void util_formatar_data(const char *iso, char *destino, size_t tamanho);
void util_titulo(const char *titulo);
void util_sucesso(const char *mensagem);
void util_erro(const char *mensagem);
void util_pausar(void);
const char *util_texto_resultado(Resultado resultado);

#endif
