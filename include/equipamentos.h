#ifndef EQUIPAMENTOS_H
#define EQUIPAMENTOS_H

#include "db.h"

typedef struct Equipamento {
    int id;
    int cliente_id;
    char cliente_nome[101];
    char tipo[12];
    char marca[61];
    char modelo[61];
    char numero_serie[61];
} Equipamento;

Resultado equipamento_validar(const Equipamento *equipamento);
const char *equipamento_tipo_rotulo(const char *codigo);
const char *equipamento_tipo_codigo(int opcao);
Resultado equipamento_inserir(sqlite3 *db, const Equipamento *equipamento,
                              int *id_novo);
Resultado equipamento_buscar_por_id(sqlite3 *db, int id,
                                    Equipamento *saida);
Resultado equipamento_listar(sqlite3 *db, Equipamento **lista,
                             int *quantidade);
Resultado equipamento_listar_por_cliente(sqlite3 *db, int cliente_id,
                                         Equipamento **lista,
                                         int *quantidade);
void menu_equipamentos(sqlite3 *db);

#endif
