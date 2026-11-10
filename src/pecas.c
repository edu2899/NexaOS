/*
 * Implementa cadastro, consulta e movimentacao de pecas.
 * Mantem o estoque sincronizado com o historico de movimentacoes.
 */
#include "pecas.h"

#include "util.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void peca_normalizar_codigo(char *destino, size_t tamanho,
                                  const char *codigo)
{
    size_t i;

    snprintf(destino, tamanho, "%s", codigo != NULL ? codigo : "");
    for (i = 0; destino[i] != '\0'; i++) {
        destino[i] = (char)toupper((unsigned char)destino[i]);
    }
}

static void peca_ler_colunas(sqlite3_stmt *consulta, Peca *peca)
{
    peca->id = sqlite3_column_int(consulta, 0);
    db_copiar_coluna(peca->codigo, sizeof(peca->codigo), consulta, 1);
    db_copiar_coluna(peca->nome, sizeof(peca->nome), consulta, 2);
    peca->quantidade = sqlite3_column_int(consulta, 3);
    peca->estoque_minimo = sqlite3_column_int(consulta, 4);
}

Resultado peca_validar(const Peca *peca)
{
    if (peca == NULL || peca->codigo[0] == '\0' || peca->nome[0] == '\0' ||
        peca->quantidade < 0 || peca->quantidade > 100000 ||
        peca->estoque_minimo < 0 || peca->estoque_minimo > 100000) {
        return RES_INVALIDO;
    }
    return RES_OK;
}

static Resultado peca_inserir_movimento(sqlite3 *db, int peca_id,
                                        const char *tipo, int quantidade,
                                        int ordem_id, const char *motivo)
{
    static const char sql[] =
        "INSERT INTO movimentacoes_estoque "
        "(peca_id, ordem_id, tipo, quantidade, motivo) "
        "VALUES (?1, ?2, ?3, ?4, ?5);";
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 1, peca_id);
    if (codigo == SQLITE_OK) {
        codigo = ordem_id == 0 ? sqlite3_bind_null(consulta, 2)
                               : sqlite3_bind_int(consulta, 2, ordem_id);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 3, tipo, 0);
    }
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 4, quantidade);
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 5, motivo, 1);
    }
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    return db_finalizar(db, consulta,
                        codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db));
}

static Resultado peca_reverter(sqlite3 *db, Resultado resultado)
{
    (void)db_desfazer(db);
    return resultado;
}

static Resultado peca_inserir(sqlite3 *db, const Peca *peca, int *id_novo)
{
    static const char sql[] =
        "INSERT INTO pecas (codigo, nome, quantidade, estoque_minimo) "
        "VALUES (?1, ?2, ?3, ?4);";
    sqlite3_stmt *consulta = NULL;
    char codigo_normalizado[sizeof(peca->codigo)];
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    peca_normalizar_codigo(codigo_normalizado, sizeof(codigo_normalizado),
                           peca->codigo);
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 1, codigo_normalizado, 0);
    }
    if (codigo == SQLITE_OK) codigo = db_vincular_texto(consulta, 2, peca->nome, 0);
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 3, peca->quantidade);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_bind_int(consulta, 4, peca->estoque_minimo);
    }
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (codigo != SQLITE_DONE) {
        return db_finalizar(db, consulta, db_traduzir_erro(db));
    }
    *id_novo = (int)sqlite3_last_insert_rowid(db);
    return db_finalizar(db, consulta, RES_OK);
}

Resultado peca_cadastrar(sqlite3 *db, const Peca *peca, int *id_novo)
{
    Resultado resultado;

    if (id_novo != NULL) *id_novo = 0;
    if (db == NULL || id_novo == NULL) return RES_INVALIDO;
    resultado = peca_validar(peca);
    if (resultado != RES_OK) return resultado;
    resultado = db_iniciar_transacao(db);
    if (resultado != RES_OK) return resultado;
    resultado = peca_inserir(db, peca, id_novo);
    if (resultado == RES_OK && peca->quantidade > 0) {
        resultado = peca_inserir_movimento(db, *id_novo, "ENTRADA",
                                            peca->quantidade, 0,
                                            "Estoque inicial");
    }
    if (resultado != RES_OK) {
        *id_novo = 0;
        return peca_reverter(db, resultado);
    }
    resultado = db_confirmar(db);
    if (resultado != RES_OK) {
        *id_novo = 0;
        return peca_reverter(db, resultado);
    }
    return RES_OK;
}

static Resultado peca_buscar(sqlite3 *db, const char *sql, int por_codigo,
                             int id, const char *codigo_texto, Peca *saida)
{
    sqlite3_stmt *consulta = NULL;
    char codigo_normalizado[31];
    int codigo;

    if (db == NULL || saida == NULL) return RES_INVALIDO;
    if (por_codigo) {
        if (codigo_texto == NULL || codigo_texto[0] == '\0') return RES_INVALIDO;
        peca_normalizar_codigo(codigo_normalizado, sizeof(codigo_normalizado),
                               codigo_texto);
    } else if (id <= 0) {
        return RES_INVALIDO;
    }
    codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);
    if (codigo == SQLITE_OK) {
        codigo = por_codigo
            ? db_vincular_texto(consulta, 1, codigo_normalizado, 0)
            : sqlite3_bind_int(consulta, 1, id);
    }
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (codigo == SQLITE_ROW) {
        peca_ler_colunas(consulta, saida);
        return db_finalizar(db, consulta, RES_OK);
    }
    return db_finalizar(db, consulta, codigo == SQLITE_DONE
        ? RES_NAO_ENCONTRADO : db_traduzir_erro(db));
}

Resultado peca_buscar_por_id(sqlite3 *db, int id, Peca *saida)
{
    static const char sql[] =
        "SELECT id, codigo, nome, quantidade, estoque_minimo "
        "FROM pecas WHERE id = ?1;";

    return peca_buscar(db, sql, 0, id, NULL, saida);
}

Resultado peca_buscar_por_codigo(sqlite3 *db, const char *codigo,
                                 Peca *saida)
{
    static const char sql[] =
        "SELECT id, codigo, nome, quantidade, estoque_minimo "
        "FROM pecas WHERE codigo = ?1;";

    return peca_buscar(db, sql, 1, 0, codigo, saida);
}

static Resultado peca_adicionar_linha(sqlite3_stmt *consulta, Peca **lista,
                                      size_t *capacidade, int quantidade)
{
    if ((size_t)quantidade == *capacidade) {
        Peca *temporario = util_dobrar_capacidade(
            *lista, capacidade, sizeof(**lista));
        if (temporario == NULL) return RES_ERRO_MEMORIA;
        *lista = temporario;
    }
    peca_ler_colunas(consulta, &(*lista)[quantidade]);
    return RES_OK;
}

static Resultado peca_ler_linhas(sqlite3 *db, sqlite3_stmt *consulta,
                                 Peca **lista, size_t *capacidade, int *total)
{
    int codigo;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        Resultado resultado;
        if (*total == INT_MAX) return RES_ERRO_MEMORIA;
        resultado = peca_adicionar_linha(consulta, lista, capacidade, *total);
        if (resultado != RES_OK) return resultado;
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado peca_listar_com_filtro(sqlite3 *db, int baixo,
                                        Peca **lista, int *quantidade)
{
    static const char sql_todas[] =
        "SELECT id, codigo, nome, quantidade, estoque_minimo FROM pecas "
        "ORDER BY codigo;";
    static const char sql_baixo[] =
        "SELECT id, codigo, nome, quantidade, estoque_minimo FROM pecas "
        "WHERE quantidade <= estoque_minimo ORDER BY codigo;";
    sqlite3_stmt *consulta = NULL;
    Peca *pecas;
    size_t capacidade = 8;
    int total = 0;
    Resultado resultado;

    if (db == NULL || lista == NULL || quantidade == NULL) return RES_INVALIDO;
    *lista = NULL;
    *quantidade = 0;
    pecas = malloc(capacidade * sizeof(*pecas));
    if (pecas == NULL) return RES_ERRO_MEMORIA;
    resultado = sqlite3_prepare_v2(db, baixo ? sql_baixo : sql_todas, -1,
                                   &consulta, NULL) == SQLITE_OK
        ? RES_OK : db_traduzir_erro(db);
    if (resultado == RES_OK) {
        resultado = peca_ler_linhas(db, consulta, &pecas, &capacidade, &total);
    }
    if (consulta != NULL) resultado = db_finalizar(db, consulta, resultado);
    if (resultado != RES_OK) {
        free(pecas);
        return resultado;
    }
    if (total == 0) free(pecas);
    else *lista = pecas;
    *quantidade = total;
    return RES_OK;
}

Resultado peca_listar(sqlite3 *db, Peca **lista, int *quantidade)
{
    return peca_listar_com_filtro(db, 0, lista, quantidade);
}

Resultado peca_listar_estoque_baixo(sqlite3 *db, Peca **lista,
                                    int *quantidade)
{
    return peca_listar_com_filtro(db, 1, lista, quantidade);
}

static Resultado peca_atualizar_quantidade(sqlite3 *db, int peca_id,
                                           int quantidade, int entrada)
{
    const char *sql = entrada
        ? "UPDATE pecas SET quantidade = quantidade + ?1 WHERE id = ?2;"
        : "UPDATE pecas SET quantidade = quantidade - ?1 "
          "WHERE id = ?2 AND quantidade >= ?1;";
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 1, quantidade);
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 2, peca_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (codigo != SQLITE_DONE) {
        return db_finalizar(db, consulta, db_traduzir_erro(db));
    }
    codigo = sqlite3_changes(db);
    if (sqlite3_finalize(consulta) != SQLITE_OK) return db_traduzir_erro(db);
    return codigo == 0 ? RES_NAO_ENCONTRADO : RES_OK;
}

Resultado peca_registrar_entrada(sqlite3 *db, int peca_id, int quantidade,
                                 const char *motivo)
{
    Resultado resultado;

    if (db == NULL || peca_id <= 0 || quantidade < 1 || quantidade > 100000 ||
        (motivo != NULL && strlen(motivo) > 200)) return RES_INVALIDO;
    resultado = db_iniciar_transacao(db);
    if (resultado != RES_OK) return resultado;
    resultado = peca_atualizar_quantidade(db, peca_id, quantidade, 1);
    if (resultado == RES_OK) {
        resultado = peca_inserir_movimento(db, peca_id, "ENTRADA", quantidade,
                                            0, motivo);
    }
    if (resultado != RES_OK) return peca_reverter(db, resultado);
    resultado = db_confirmar(db);
    return resultado == RES_OK ? RES_OK : peca_reverter(db, resultado);
}

Resultado peca_saida_em_transacao(sqlite3 *db, int peca_id, int quantidade,
                                  int ordem_id, const char *motivo)
{
    Resultado resultado;
    Peca peca;

    if (db == NULL || peca_id <= 0 || quantidade < 1 || quantidade > 100000 ||
        ordem_id < 0 || (motivo != NULL && strlen(motivo) > 200)) {
        return RES_INVALIDO;
    }
    resultado = peca_atualizar_quantidade(db, peca_id, quantidade, 0);
    if (resultado == RES_NAO_ENCONTRADO) {
        resultado = peca_buscar_por_id(db, peca_id, &peca);
        return resultado == RES_OK ? RES_ESTOQUE_INSUFICIENTE : resultado;
    }
    if (resultado != RES_OK) return resultado;
    return peca_inserir_movimento(db, peca_id, "SAIDA", quantidade, ordem_id,
                                  motivo);
}

Resultado peca_registrar_saida(sqlite3 *db, int peca_id, int quantidade,
                               const char *motivo)
{
    Resultado resultado;

    if (motivo == NULL || motivo[0] == '\0' || strlen(motivo) > 200) {
        return RES_INVALIDO;
    }
    if (db == NULL || peca_id <= 0 || quantidade < 1 || quantidade > 100000) {
        return RES_INVALIDO;
    }
    resultado = db_iniciar_transacao(db);
    if (resultado != RES_OK) return resultado;
    resultado = peca_saida_em_transacao(db, peca_id, quantidade, 0, motivo);
    if (resultado != RES_OK) return peca_reverter(db, resultado);
    resultado = db_confirmar(db);
    return resultado == RES_OK ? RES_OK : peca_reverter(db, resultado);
}

static void movimento_ler_colunas(sqlite3_stmt *consulta,
                                  MovimentoEstoque *movimento)
{
    movimento->id = sqlite3_column_int(consulta, 0);
    movimento->peca_id = sqlite3_column_int(consulta, 1);
    db_copiar_coluna(movimento->peca_codigo, sizeof(movimento->peca_codigo),
                     consulta, 2);
    db_copiar_coluna(movimento->peca_nome, sizeof(movimento->peca_nome),
                     consulta, 3);
    movimento->ordem_id = sqlite3_column_int(consulta, 4);
    db_copiar_coluna(movimento->tipo, sizeof(movimento->tipo), consulta, 5);
    movimento->quantidade = sqlite3_column_int(consulta, 6);
    db_copiar_coluna(movimento->motivo, sizeof(movimento->motivo), consulta, 7);
    db_copiar_coluna(movimento->data_hora, sizeof(movimento->data_hora),
                     consulta, 8);
}

static Resultado movimento_listar_linhas(sqlite3 *db, sqlite3_stmt *consulta,
                                         MovimentoEstoque **lista,
                                         size_t *capacidade, int *total)
{
    int codigo;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        if ((size_t)*total == *capacidade) {
            MovimentoEstoque *temporario = util_dobrar_capacidade(
                *lista, capacidade, sizeof(**lista));
            if (temporario == NULL) return RES_ERRO_MEMORIA;
            *lista = temporario;
        }
        if (*total == INT_MAX) return RES_ERRO_MEMORIA;
        movimento_ler_colunas(consulta, &(*lista)[*total]);
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado movimento_preparar(sqlite3 *db, int peca_id,
                                    sqlite3_stmt **consulta)
{
    static const char sql_todas[] =
        "SELECT m.id, m.peca_id, p.codigo, p.nome, COALESCE(m.ordem_id, 0), "
        "m.tipo, m.quantidade, m.motivo, m.data_hora "
        "FROM movimentacoes_estoque m JOIN pecas p ON p.id = m.peca_id "
        "ORDER BY m.id;";
    static const char sql_peca[] =
        "SELECT m.id, m.peca_id, p.codigo, p.nome, COALESCE(m.ordem_id, 0), "
        "m.tipo, m.quantidade, m.motivo, m.data_hora "
        "FROM movimentacoes_estoque m JOIN pecas p ON p.id = m.peca_id "
        "WHERE m.peca_id = ?1 ORDER BY m.id;";
    const char *sql = peca_id == 0 ? sql_todas : sql_peca;
    int codigo = sqlite3_prepare_v2(db, sql, -1, consulta, NULL);

    if (codigo == SQLITE_OK && peca_id > 0) {
        codigo = sqlite3_bind_int(*consulta, 1, peca_id);
    }
    return codigo == SQLITE_OK ? RES_OK : db_traduzir_erro(db);
}

static void movimento_publicar(MovimentoEstoque *movimentos, int total,
                               MovimentoEstoque **lista, int *quantidade)
{
    if (total == 0) free(movimentos);
    else *lista = movimentos;
    *quantidade = total;
}

Resultado estoque_listar_movimentacoes(sqlite3 *db, int peca_id,
                                       MovimentoEstoque **lista,
                                       int *quantidade)
{
    sqlite3_stmt *consulta = NULL;
    MovimentoEstoque *movimentos;
    size_t capacidade = 8;
    int total = 0;
    Resultado resultado;

    if (db == NULL || peca_id < 0 || lista == NULL || quantidade == NULL) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;
    movimentos = malloc(capacidade * sizeof(*movimentos));
    if (movimentos == NULL) return RES_ERRO_MEMORIA;
    resultado = movimento_preparar(db, peca_id, &consulta);
    if (resultado == RES_OK) {
        resultado = movimento_listar_linhas(db, consulta, &movimentos,
                                            &capacidade, &total);
    }
    if (consulta != NULL) resultado = db_finalizar(db, consulta, resultado);
    if (resultado != RES_OK) {
        free(movimentos);
        return resultado;
    }
    movimento_publicar(movimentos, total, lista, quantidade);
    return RES_OK;
}
