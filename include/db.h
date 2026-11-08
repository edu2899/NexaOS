#ifndef DB_H
#define DB_H

#include <sqlite3.h>
#include "resultado.h"

Resultado db_abrir(sqlite3 **db, const char *caminho);
Resultado db_executar_script(sqlite3 *db, const char *arquivo);
void db_fechar(sqlite3 *db);
Resultado db_iniciar_transacao(sqlite3 *db);
Resultado db_confirmar(sqlite3 *db);
Resultado db_desfazer(sqlite3 *db);
Resultado db_traduzir_erro(sqlite3 *db);

#endif
