#ifndef RELATORIOS_H
#define RELATORIOS_H

#include "ordens.h"

typedef struct ResumoOrdens {
    int total;
    /* Em aberto exclui CONCLUIDA, ENTREGUE e CANCELADA. */
    int em_aberto;
    /* Concluídas inclui CONCLUIDA e ENTREGUE. */
    int concluidas;
    int por_status[OS_STATUS_TOTAL];
} ResumoOrdens;

Resultado relatorio_resumo_ordens(sqlite3 *db, ResumoOrdens *saida);
Resultado relatorio_ordens_por_mes(sqlite3 *db, int ano,
                                   int matriz[12][OS_STATUS_TOTAL]);
int relatorio_total_mes(int matriz[12][OS_STATUS_TOTAL], int mes);
int relatorio_total_status(int matriz[12][OS_STATUS_TOTAL], int status);

#endif
