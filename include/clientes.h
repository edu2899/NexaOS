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

/* Valida os campos de um cliente. */
Resultado cliente_validar(const Cliente *cliente);
/* Insere um cliente e devolve o identificador gerado. */
Resultado cliente_inserir(sqlite3 *db, const Cliente *cliente, int *id_novo);
/* Busca um cliente pelo identificador. */
Resultado cliente_buscar_por_id(sqlite3 *db, int id, Cliente *saida);
/* Atualiza os dados de um cliente existente. */
Resultado cliente_atualizar(sqlite3 *db, const Cliente *cliente);
/* Devolve todos os clientes em um vetor alocado. */
Resultado cliente_listar(sqlite3 *db, Cliente **lista, int *quantidade);
/* Executa o menu de clientes. */
void menu_clientes(sqlite3 *db);

#endif
