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

/* Valida os campos de um equipamento. */
Resultado equipamento_validar(const Equipamento *equipamento);
/* Devolve o rótulo de um tipo de equipamento. */
const char *equipamento_tipo_rotulo(const char *codigo);
/* Devolve o código do tipo escolhido no menu. */
const char *equipamento_tipo_codigo(int opcao);
/* Insere um equipamento e devolve o identificador gerado. */
Resultado equipamento_inserir(sqlite3 *db, const Equipamento *equipamento,
                              int *id_novo);
/* Busca um equipamento pelo identificador. */
Resultado equipamento_buscar_por_id(sqlite3 *db, int id,
                                    Equipamento *saida);
/* Devolve todos os equipamentos em um vetor alocado. */
Resultado equipamento_listar(sqlite3 *db, Equipamento **lista,
                             int *quantidade);
/* Lista equipamentos associados a um cliente. */
Resultado equipamento_listar_por_cliente(sqlite3 *db, int cliente_id,
                                         Equipamento **lista,
                                         int *quantidade);
/* Executa o menu de equipamentos. */
void menu_equipamentos(sqlite3 *db);

#endif
