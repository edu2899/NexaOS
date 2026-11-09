#include "test_util.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQUIVO_ENTRADA "build\\test_validacao_input.txt"

static int redirecionar_entrada(const char *texto)
{
    FILE *arquivo = fopen(ARQUIVO_ENTRADA, "w");

    if (arquivo == NULL) {
        return 0;
    }
    if (fputs(texto, arquivo) == EOF) {
        fclose(arquivo);
        return 0;
    }
    if (fclose(arquivo) != 0) {
        return 0;
    }

    return freopen(ARQUIVO_ENTRADA, "r", stdin) != NULL;
}

static void testar_texto_e_validacoes(void)
{
    char texto[] = " \tNexaOS \r\n";
    char digitos[16];

    util_trim(texto);
    TESTE_VERIFICAR(strcmp(texto, "NexaOS") == 0, "util_trim remove espacos");

    util_somente_digitos("(11) 99999-9999", digitos, sizeof(digitos));
    TESTE_VERIFICAR(strcmp(digitos, "11999999999") == 0,
                    "util_somente_digitos filtra caracteres");

    TESTE_VERIFICAR(util_validar_documento("12345678901"),
                    "documento com 11 digitos");
    TESTE_VERIFICAR(util_validar_documento("12345678000199"),
                    "documento com 14 digitos");
    TESTE_VERIFICAR(!util_validar_documento("1234567890"),
                    "documento com 10 digitos invalido");
    TESTE_VERIFICAR(!util_validar_documento("1234567890a"),
                    "documento com letras invalido");
    TESTE_VERIFICAR(!util_validar_documento(""),
                    "documento vazio invalido");

    TESTE_VERIFICAR(util_validar_telefone("1199999999"),
                    "telefone com 10 digitos");
    TESTE_VERIFICAR(util_validar_telefone("11999999999"),
                    "telefone com 11 digitos");
    TESTE_VERIFICAR(!util_validar_telefone("119999999"),
                    "telefone com 9 digitos invalido");
    TESTE_VERIFICAR(!util_validar_telefone("119999999999"),
                    "telefone com 12 digitos invalido");

    TESTE_VERIFICAR(util_validar_email("a@b.com"), "email valido");
    TESTE_VERIFICAR(!util_validar_email("a@b"), "email sem ponto no dominio");
    TESTE_VERIFICAR(!util_validar_email("@b.com"), "email sem usuario");
    TESTE_VERIFICAR(!util_validar_email("a b@c.com"), "email com espaco");
    TESTE_VERIFICAR(!util_validar_email("a@@b.com"), "email com dois arrobas");
    TESTE_VERIFICAR(!util_validar_email("a@.com"), "email com ponto inicial");
    TESTE_VERIFICAR(!util_validar_email("a@b."), "email com ponto final");
}

static void testar_entrada(void)
{
    char linha[5];
    char curta[16];
    int valor = -1;
    Resultado resultado;

    TESTE_VERIFICAR(redirecionar_entrada("abc\n9\n3\n"),
                    "preparar entrada de inteiro");
    resultado = util_ler_inteiro("Inteiro de teste: ", 0, 7, &valor);
    TESTE_VERIFICAR(resultado == RES_OK && valor == 3,
                    "inteiro repete ate receber valor valido");

    TESTE_VERIFICAR(redirecionar_entrada("abcdefghijk\nok\n"),
                    "preparar entrada de linhas");
    resultado = util_ler_linha(NULL, linha, sizeof(linha));
    TESTE_VERIFICAR(resultado == RES_OK && strcmp(linha, "abcd") == 0,
                    "linha longa e truncada");
    resultado = util_ler_linha(NULL, curta, sizeof(curta));
    TESTE_VERIFICAR(resultado == RES_OK && strcmp(curta, "ok") == 0,
                    "restante da linha longa e descartado");

    TESTE_VERIFICAR(redirecionar_entrada(""),
                    "preparar entrada vazia");
    resultado = util_ler_linha(NULL, curta, sizeof(curta));
    TESTE_VERIFICAR(resultado == RES_ERRO_ARQUIVO,
                    "EOF retorna erro de arquivo");
}

static void testar_dobrar_capacidade(void)
{
    size_t capacidade = 4;
    int *valores = malloc(capacidade * sizeof(*valores));
    int *temporario;
    int i;

    TESTE_VERIFICAR(valores != NULL, "alocar vetor para crescimento");
    if (valores == NULL) {
        return;
    }
    for (i = 0; i < 4; i++) {
        valores[i] = (i + 1) * 10;
    }
    temporario = util_dobrar_capacidade(valores, &capacidade,
                                        sizeof(*valores));
    TESTE_VERIFICAR(temporario != NULL && capacidade == 8,
                    "dobrar capacidade de 4 para 8");
    if (temporario != NULL) {
        valores = temporario;
    }
    if (capacidade == 8) {
        for (i = 0; i < 4; i++) {
            TESTE_VERIFICAR(valores[i] == (i + 1) * 10,
                            "preservar dados ao dobrar capacidade");
        }
    }
    free(valores);
}

static void testar_formatar_data(void)
{
    char data[32];

    util_formatar_data("2026-11-09 14:30:05", data, sizeof(data));
    TESTE_VERIFICAR(strcmp(data, "09/11/2026 14:30:05") == 0,
                    "formatar data ISO");
    util_formatar_data("data desconhecida", data, sizeof(data));
    TESTE_VERIFICAR(strcmp(data, "data desconhecida") == 0,
                    "preservar formato de data desconhecido");
}

int main(void)
{
    int fechamento_ok;

    testar_texto_e_validacoes();
    testar_dobrar_capacidade();
    testar_formatar_data();
    testar_entrada();

    fechamento_ok = fclose(stdin) == 0;
    TESTE_VERIFICAR(fechamento_ok, "fechar entrada temporaria");
    if (remove(ARQUIVO_ENTRADA) != 0) {
        perror(ARQUIVO_ENTRADA);
        TESTE_VERIFICAR(0, "remover arquivo temporario");
    }

    return teste_codigo_saida();
}
