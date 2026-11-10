#ifndef ITENS_ORDEM_H
#define ITENS_ORDEM_H

#include "db.h"

typedef struct ItemOrdem {
    int id;
    int ordem_id;
    int peca_id;
    char peca_codigo[31];
    char peca_nome[101];
    int quantidade;
    char data_uso[20];
} ItemOrdem;

Resultado item_ordem_usar_peca(sqlite3 *db, int ordem_id, int peca_id,
                               int quantidade);
Resultado item_ordem_listar(sqlite3 *db, int ordem_id, ItemOrdem **lista,
                            int *quantidade);

#endif
