#ifndef TECNICOS_H
#define TECNICOS_H

#include "db.h"

typedef struct Tecnico {
    int id;
    char nome[101];
    char especialidade[81];
} Tecnico;

/* Valida os dados de um técnico. */
Resultado tecnico_validar(const Tecnico *tecnico);
/* Insere um técnico e devolve o identificador gerado. */
Resultado tecnico_inserir(sqlite3 *db, const Tecnico *tecnico, int *id_novo);
/* Busca um técnico pelo identificador. */
Resultado tecnico_buscar_por_id(sqlite3 *db, int id, Tecnico *saida);
/* Devolve todos os técnicos em um vetor alocado. */
Resultado tecnico_listar(sqlite3 *db, Tecnico **lista, int *quantidade);
/* Executa o menu de técnicos. */
void menu_tecnicos(sqlite3 *db);

#endif
