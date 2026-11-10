#include "itens_ordem.h"

#include "historico.h"
#include "ordens.h"
#include "pecas.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int item_situacao_permite_uso(const char *status)
{
    return strcmp(status, "APROVADA") == 0 ||
           strcmp(status, "AGUARDANDO_PECA") == 0 ||
           strcmp(status, "EM_REPARO") == 0;
}

static Resultado item_reverter(sqlite3 *db, Resultado resultado)
{
    (void)db_desfazer(db);
    return resultado;
}

static Resultado item_inserir(sqlite3 *db, int ordem_id, int peca_id,
                              int quantidade)
{
    static const char sql[] =
        "INSERT INTO itens_ordem (ordem_id, peca_id, quantidade) "
        "VALUES (?1, ?2, ?3);";
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 1, ordem_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 2, peca_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 3, quantidade);
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    return db_finalizar(db, consulta,
                        codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db));
}

static Resultado item_registrar_historico(sqlite3 *db, int ordem_id,
                                          int peca_id, int quantidade)
{
    Peca peca;
    char descricao[501];
    Resultado resultado = peca_buscar_por_id(db, peca_id, &peca);

    if (resultado != RES_OK) return resultado;
    snprintf(descricao, sizeof(descricao), "Peça usada: %s - %s (x%d)",
             peca.codigo, peca.nome, quantidade);
    return historico_registrar(db, ordem_id, "PECA", NULL, NULL, descricao);
}

Resultado item_ordem_usar_peca(sqlite3 *db, int ordem_id, int peca_id,
                               int quantidade)
{
    OrdemServico ordem;
    char motivo[64];
    Resultado resultado;

    if (db == NULL || ordem_id <= 0 || peca_id <= 0 || quantidade < 1 ||
        quantidade > 100000) return RES_INVALIDO;
    resultado = db_iniciar_transacao(db);
    if (resultado != RES_OK) return resultado;
    resultado = ordem_buscar_por_id(db, ordem_id, &ordem);
    if (resultado == RES_OK && !item_situacao_permite_uso(ordem.status)) {
        resultado = RES_REGRA_NEGOCIO;
    }
    if (resultado == RES_OK) {
        snprintf(motivo, sizeof(motivo), "Uso na OS %d", ordem_id);
        resultado = peca_saida_em_transacao(db, peca_id, quantidade, ordem_id,
                                             motivo);
    }
    if (resultado == RES_OK) {
        resultado = item_inserir(db, ordem_id, peca_id, quantidade);
    }
    if (resultado == RES_OK) {
        resultado = item_registrar_historico(db, ordem_id, peca_id, quantidade);
    }
    if (resultado != RES_OK) return item_reverter(db, resultado);
    resultado = db_confirmar(db);
    return resultado == RES_OK ? RES_OK : item_reverter(db, resultado);
}

static void item_ler_colunas(sqlite3_stmt *consulta, ItemOrdem *item)
{
    item->id = sqlite3_column_int(consulta, 0);
    item->ordem_id = sqlite3_column_int(consulta, 1);
    item->peca_id = sqlite3_column_int(consulta, 2);
    db_copiar_coluna(item->peca_codigo, sizeof(item->peca_codigo), consulta, 3);
    db_copiar_coluna(item->peca_nome, sizeof(item->peca_nome), consulta, 4);
    item->quantidade = sqlite3_column_int(consulta, 5);
    db_copiar_coluna(item->data_uso, sizeof(item->data_uso), consulta, 6);
}

static Resultado item_ler_linhas(sqlite3 *db, sqlite3_stmt *consulta,
                                 ItemOrdem **lista, size_t *capacidade,
                                 int *total)
{
    int codigo;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        if (*total == INT_MAX) return RES_ERRO_MEMORIA;
        if ((size_t)*total == *capacidade) {
            ItemOrdem *temporario = util_dobrar_capacidade(
                *lista, capacidade, sizeof(**lista));
            if (temporario == NULL) return RES_ERRO_MEMORIA;
            *lista = temporario;
        }
        item_ler_colunas(consulta, &(*lista)[*total]);
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado item_preparar_consulta(sqlite3 *db, int ordem_id,
                                        sqlite3_stmt **consulta)
{
    static const char sql[] =
        "SELECT i.id, i.ordem_id, i.peca_id, p.codigo, p.nome, "
        "i.quantidade, i.data_uso FROM itens_ordem i "
        "JOIN pecas p ON p.id = i.peca_id WHERE i.ordem_id = ?1 ORDER BY i.id;";
    int codigo = sqlite3_prepare_v2(db, sql, -1, consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(*consulta, 1, ordem_id);
    return codigo == SQLITE_OK ? RES_OK : db_traduzir_erro(db);
}

static void item_publicar_lista(ItemOrdem *itens, int total,
                                ItemOrdem **lista, int *quantidade)
{
    if (total == 0) free(itens);
    else *lista = itens;
    *quantidade = total;
}

Resultado item_ordem_listar(sqlite3 *db, int ordem_id, ItemOrdem **lista,
                            int *quantidade)
{
    sqlite3_stmt *consulta = NULL;
    ItemOrdem *itens;
    size_t capacidade = 8;
    int total = 0;
    Resultado resultado;

    if (db == NULL || ordem_id <= 0 || lista == NULL || quantidade == NULL) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;
    {
        OrdemServico ordem;
        resultado = ordem_buscar_por_id(db, ordem_id, &ordem);
    }
    if (resultado != RES_OK) return resultado;
    itens = malloc(capacidade * sizeof(*itens));
    if (itens == NULL) return RES_ERRO_MEMORIA;
    resultado = item_preparar_consulta(db, ordem_id, &consulta);
    if (resultado == RES_OK) {
        resultado = item_ler_linhas(db, consulta, &itens, &capacidade, &total);
    }
    if (consulta != NULL) resultado = db_finalizar(db, consulta, resultado);
    if (resultado != RES_OK) {
        free(itens);
        return resultado;
    }
    item_publicar_lista(itens, total, lista, quantidade);
    return RES_OK;
}
