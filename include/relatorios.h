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

/* Calcula totais de ordens por situação e grupo de encerramento. */
Resultado relatorio_resumo_ordens(sqlite3 *db, ResumoOrdens *saida);
/* Preenche a matriz de ordens por mês e situação para um ano. */
Resultado relatorio_ordens_por_mes(sqlite3 *db, int ano,
                                   int matriz[12][OS_STATUS_TOTAL]);
/* Soma as situações de um mês válido. */
int relatorio_total_mes(int matriz[12][OS_STATUS_TOTAL], int mes);
/* Soma os meses de uma situação válida. */
int relatorio_total_status(int matriz[12][OS_STATUS_TOTAL], int status);

#endif
