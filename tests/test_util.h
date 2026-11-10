#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <sqlite3.h>

long vazamento_pendentes(void);
void vazamento_iniciar(void);

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
    int codigo_sqlite = sqlite3_shutdown();
    long pendentes = vazamento_pendentes();

    if (codigo_sqlite == SQLITE_OK && pendentes == 0) {
        printf("[OK] sem vazamento de memoria\n");
    } else {
        printf("[FALHA] sem vazamento de memoria (%ld blocos; SQLite %d)\n",
               pendentes, codigo_sqlite);
        teste_falhas++;
    }
    return teste_falhas == 0 ? 0 : 1;
}

#endif
