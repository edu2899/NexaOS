#ifndef EXPORTACAO_H
#define EXPORTACAO_H

#include "db.h"

Resultado exportar_resumo_ordens(sqlite3 *db, const char *caminho);
Resultado exportar_ordens_por_mes(sqlite3 *db, int ano,
                                  const char *caminho);
Resultado exportar_estoque_baixo(sqlite3 *db, const char *caminho);
Resultado exportar_movimentacoes(sqlite3 *db, const char *caminho);

#endif
