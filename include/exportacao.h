#ifndef EXPORTACAO_H
#define EXPORTACAO_H

#include "db.h"

/* Exporta totais de ordens por situação para CSV. */
Resultado exportar_resumo_ordens(sqlite3 *db, const char *caminho);
/* Exporta ordens agrupadas por mês para CSV. */
Resultado exportar_ordens_por_mes(sqlite3 *db, int ano,
                                  const char *caminho);
/* Exporta peças com estoque baixo para CSV. */
Resultado exportar_estoque_baixo(sqlite3 *db, const char *caminho);
/* Exporta todas as movimentações de estoque para CSV. */
Resultado exportar_movimentacoes(sqlite3 *db, const char *caminho);

#endif
