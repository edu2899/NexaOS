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

/* Valida os dados de uma peça. */
Resultado peca_validar(const Peca *peca);
/* Cadastra uma peça e registra seu estoque inicial. */
Resultado peca_cadastrar(sqlite3 *db, const Peca *peca, int *id_novo);
/* Busca uma peça pelo identificador. */
Resultado peca_buscar_por_id(sqlite3 *db, int id, Peca *saida);
/* Busca uma peça pelo código normalizado. */
Resultado peca_buscar_por_codigo(sqlite3 *db, const char *codigo,
                                 Peca *saida);
/* Lista todas as peças em ordem de código. */
Resultado peca_listar(sqlite3 *db, Peca **lista, int *quantidade);
/* Lista as peças no limite ou abaixo do estoque mínimo. */
Resultado peca_listar_estoque_baixo(sqlite3 *db, Peca **lista,
                                    int *quantidade);
/* Registra uma entrada de estoque em uma transação própria. */
Resultado peca_registrar_entrada(sqlite3 *db, int peca_id, int quantidade,
                                 const char *motivo);
/* Registra uma saída sem controlar a transação do chamador. */
Resultado peca_saida_em_transacao(sqlite3 *db, int peca_id, int quantidade,
                                  int ordem_id, const char *motivo);
/* Registra uma saída manual em uma transação própria. */
Resultado peca_registrar_saida(sqlite3 *db, int peca_id, int quantidade,
                               const char *motivo);
/* Lista movimentações de uma peça ou de todo o estoque. */
Resultado estoque_listar_movimentacoes(sqlite3 *db, int peca_id,
                                       MovimentoEstoque **lista,
                                       int *quantidade);

#endif
