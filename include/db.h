#ifndef DB_H
#define DB_H

#include <stddef.h>
#include <sqlite3.h>
#include "resultado.h"

/* Abre o banco, ativa chaves estrangeiras e aplica o esquema. */
Resultado db_abrir(sqlite3 **db, const char *caminho);
/* Lê e executa um arquivo SQL. */
Resultado db_executar_script(sqlite3 *db, const char *arquivo);
/* Fecha o banco quando ele está aberto. */
void db_fechar(sqlite3 *db);
/* Inicia uma transação de escrita. */
Resultado db_iniciar_transacao(sqlite3 *db);
/* Confirma a transação atual. */
Resultado db_confirmar(sqlite3 *db);
/* Desfaz a transação atual. */
Resultado db_desfazer(sqlite3 *db);
/* Converte um erro SQLite para o resultado correspondente. */
Resultado db_traduzir_erro(sqlite3 *db);
/* Copia uma coluna de texto para um vetor limitado. */
void db_copiar_coluna(char *destino, size_t tamanho,
                      sqlite3_stmt *consulta, int coluna);
/* Finaliza a consulta e preserva o erro anterior quando houver. */
Resultado db_finalizar(sqlite3 *db, sqlite3_stmt *consulta,
                       Resultado resultado);
/* Vincula um texto como valor obrigatório ou opcional. */
int db_vincular_texto(sqlite3_stmt *consulta, int indice, const char *texto,
                      int opcional);

#endif
