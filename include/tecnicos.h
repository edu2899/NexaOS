#ifndef TECNICOS_H
#define TECNICOS_H

#include "db.h"

typedef struct Tecnico {
    int id;
    char nome[101];
    char especialidade[81];
} Tecnico;

Resultado tecnico_validar(const Tecnico *tecnico);
Resultado tecnico_inserir(sqlite3 *db, const Tecnico *tecnico, int *id_novo);
Resultado tecnico_buscar_por_id(sqlite3 *db, int id, Tecnico *saida);
Resultado tecnico_listar(sqlite3 *db, Tecnico **lista, int *quantidade);
void menu_tecnicos(sqlite3 *db);

#endif
