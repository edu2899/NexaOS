#ifndef DB_H
#define DB_H

#include <stddef.h>
#include <sqlite3.h>
#include "resultado.h"

Resultado db_abrir(sqlite3 **db, const char *caminho);
Resultado db_executar_script(sqlite3 *db, const char *arquivo);
void db_fechar(sqlite3 *db);
Resultado db_iniciar_transacao(sqlite3 *db);
Resultado db_confirmar(sqlite3 *db);
Resultado db_desfazer(sqlite3 *db);
Resultado db_traduzir_erro(sqlite3 *db);
void db_copiar_coluna(char *destino, size_t tamanho,
                      sqlite3_stmt *consulta, int coluna);
Resultado db_finalizar(sqlite3 *db, sqlite3_stmt *consulta,
                       Resultado resultado);
int db_vincular_texto(sqlite3_stmt *consulta, int indice, const char *texto,
                      int opcional);

#endif
