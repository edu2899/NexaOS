#ifndef PECAS_H
#define PECAS_H

#include "db.h"

typedef struct Peca {
    int id;
    char codigo[31];
    char nome[101];
    int quantidade;
    int estoque_minimo;
} Peca;

typedef struct MovimentoEstoque {
    int id;
    int peca_id;
    char peca_codigo[31];
    char peca_nome[101];
    int ordem_id;
    char tipo[8];
    int quantidade;
    char motivo[201];
    char data_hora[20];
} MovimentoEstoque;

Resultado peca_validar(const Peca *peca);
Resultado peca_cadastrar(sqlite3 *db, const Peca *peca, int *id_novo);
Resultado peca_buscar_por_id(sqlite3 *db, int id, Peca *saida);
Resultado peca_buscar_por_codigo(sqlite3 *db, const char *codigo,
                                 Peca *saida);
Resultado peca_listar(sqlite3 *db, Peca **lista, int *quantidade);
Resultado peca_listar_estoque_baixo(sqlite3 *db, Peca **lista,
                                    int *quantidade);
Resultado peca_registrar_entrada(sqlite3 *db, int peca_id, int quantidade,
                                 const char *motivo);
/* Esta função não controla a transação; quem chama deve mantê-la aberta. */
Resultado peca_saida_em_transacao(sqlite3 *db, int peca_id, int quantidade,
                                  int ordem_id, const char *motivo);
Resultado peca_registrar_saida(sqlite3 *db, int peca_id, int quantidade,
                               const char *motivo);
Resultado estoque_listar_movimentacoes(sqlite3 *db, int peca_id,
                                       MovimentoEstoque **lista,
                                       int *quantidade);

#endif
