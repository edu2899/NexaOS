#include "historico.h"

#include "util.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static const char *const historico_tipos[] = {
    "ABERTURA", "STATUS", "DIAGNOSTICO", "OBSERVACAO", "TECNICO", "PECA"
};

static int historico_tipo_valido(const char *tipo_evento)
{
    size_t i;

    if (tipo_evento == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(historico_tipos) / sizeof(historico_tipos[0]); i++) {
        if (strcmp(tipo_evento, historico_tipos[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

Resultado historico_registrar(sqlite3 *db, int ordem_id,
                              const char *tipo_evento,
                              const char *status_anterior,
                              const char *status_novo,
                              const char *descricao)
{
    static const char sql[] =
        "INSERT INTO historico_ordens "
        "(ordem_id, tipo_evento, status_anterior, status_novo, descricao) "
        "VALUES (?1, ?2, ?3, ?4, ?5);";
    sqlite3_stmt *consulta = NULL;
    int codigo;

    if (db == NULL || ordem_id <= 0 || !historico_tipo_valido(tipo_evento)) {
        return RES_INVALIDO;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    codigo = sqlite3_bind_int(consulta, 1, ordem_id);
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 2, tipo_evento, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 3, status_anterior, 1);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 4, status_novo, 1);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 5, descricao, 1);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    if (codigo != SQLITE_DONE) {
        return db_finalizar(db, consulta, db_traduzir_erro(db));
    }
    return db_finalizar(db, consulta, RES_OK);
}

static void historico_ler_colunas(sqlite3_stmt *consulta,
                                  EventoHistorico *evento)
{
    evento->id = sqlite3_column_int(consulta, 0);
    evento->ordem_id = sqlite3_column_int(consulta, 1);
    db_copiar_coluna(evento->tipo_evento, sizeof(evento->tipo_evento),
                     consulta, 2);
    db_copiar_coluna(evento->status_anterior, sizeof(evento->status_anterior),
                     consulta, 3);
    db_copiar_coluna(evento->status_novo, sizeof(evento->status_novo),
                     consulta, 4);
    db_copiar_coluna(evento->descricao, sizeof(evento->descricao), consulta,
                     5);
    db_copiar_coluna(evento->data_hora, sizeof(evento->data_hora), consulta,
                     6);
}

static Resultado historico_ler_linhas(sqlite3 *db, sqlite3_stmt *consulta,
                                      EventoHistorico **eventos,
                                      size_t *capacidade, int *total)
{
    int codigo;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        if (*total == INT_MAX) {
            return RES_ERRO_MEMORIA;
        }
        if ((size_t)*total == *capacidade) {
            EventoHistorico *temporario = util_dobrar_capacidade(
                *eventos, capacidade, sizeof(**eventos));
            if (temporario == NULL) {
                return RES_ERRO_MEMORIA;
            }
            *eventos = temporario;
        }
        historico_ler_colunas(consulta, &(*eventos)[*total]);
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado historico_preparar(sqlite3 *db, int ordem_id,
                                    sqlite3_stmt **consulta)
{
    static const char sql[] =
        "SELECT id, ordem_id, tipo_evento, status_anterior, status_novo, "
        "descricao, data_hora FROM historico_ordens "
        "WHERE ordem_id = ?1 ORDER BY id;";
    int codigo = sqlite3_prepare_v2(db, sql, -1, consulta, NULL);

    if (codigo == SQLITE_OK) {
        codigo = sqlite3_bind_int(*consulta, 1, ordem_id);
    }
    return codigo == SQLITE_OK ? RES_OK : db_traduzir_erro(db);
}

static Resultado historico_publicar(EventoHistorico *eventos, int total,
                                    EventoHistorico **lista, int *quantidade)
{
    if (total == 0) {
        free(eventos);
        return RES_OK;
    }
    *lista = eventos;
    *quantidade = total;
    return RES_OK;
}

Resultado historico_listar_por_ordem(sqlite3 *db, int ordem_id,
                                     EventoHistorico **lista,
                                     int *quantidade)
{
    sqlite3_stmt *consulta = NULL;
    EventoHistorico *eventos;
    size_t capacidade = 8;
    int total = 0;
    Resultado resultado;

    if (db == NULL || ordem_id <= 0 || lista == NULL || quantidade == NULL) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;
    eventos = malloc(capacidade * sizeof(*eventos));
    if (eventos == NULL) {
        return RES_ERRO_MEMORIA;
    }
    resultado = historico_preparar(db, ordem_id, &consulta);
    if (resultado != RES_OK) {
        free(eventos);
        if (consulta != NULL) {
            (void)db_finalizar(db, consulta, resultado);
        }
        return resultado;
    }
    resultado = historico_ler_linhas(db, consulta, &eventos, &capacidade,
                                     &total);
    resultado = db_finalizar(db, consulta, resultado);
    if (resultado != RES_OK) {
        free(eventos);
        return resultado;
    }
    return historico_publicar(eventos, total, lista, quantidade);
}
