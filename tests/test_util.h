#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>

static int teste_falhas = 0;

#define TESTE_VERIFICAR(condicao, descricao) \
    do { \
        if (condicao) { \
            printf("[OK] %s\n", descricao); \
        } else { \
            printf("[FALHA] %s\n", descricao); \
            teste_falhas++; \
        } \
    } while (0)

static int teste_codigo_saida(void)
{
    return teste_falhas == 0 ? 0 : 1;
}

#endif
