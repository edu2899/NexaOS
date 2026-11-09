#ifndef HISTORICO_H
#define HISTORICO_H

#include "db.h"

typedef struct EventoHistorico {
    int id;
    int ordem_id;
    char tipo_evento[16];
    char status_anterior[24];
    char status_novo[24];
    char descricao[501];
    char data_hora[20];
} EventoHistorico;

/* A transacao e controlada por quem chama esta funcao. */
Resultado historico_registrar(sqlite3 *db, int ordem_id,
                              const char *tipo_evento,
                              const char *status_anterior,
                              const char *status_novo,
                              const char *descricao);
Resultado historico_listar_por_ordem(sqlite3 *db, int ordem_id,
                                     EventoHistorico **lista,
                                     int *quantidade);

#endif
