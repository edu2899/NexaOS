#ifndef CLIENTES_H
#define CLIENTES_H

#include "db.h"

typedef struct Cliente {
    int id;
    char nome[101];
    char documento[15];
    char telefone[12];
    char email[101];
    char endereco[151];
    char data_cadastro[20];
} Cliente;

Resultado cliente_validar(const Cliente *cliente);
Resultado cliente_inserir(sqlite3 *db, const Cliente *cliente, int *id_novo);
Resultado cliente_buscar_por_id(sqlite3 *db, int id, Cliente *saida);
Resultado cliente_atualizar(sqlite3 *db, const Cliente *cliente);
Resultado cliente_listar(sqlite3 *db, Cliente **lista, int *quantidade);
void menu_clientes(sqlite3 *db);

#endif
