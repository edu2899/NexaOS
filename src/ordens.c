/*
 * Implementa abertura, consulta e atualizacao de ordens de servico.
 * Registra eventos de historico junto das alteracoes correspondentes.
 */
#include "ordens.h"

#include "equipamentos.h"
#include "historico.h"
#include "tecnicos.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const os_codigos[OS_STATUS_TOTAL] = {
    "ABERTA", "EM_DIAGNOSTICO", "AGUARDANDO_APROVACAO", "APROVADA",
    "AGUARDANDO_PECA", "EM_REPARO", "CONCLUIDA", "ENTREGUE", "CANCELADA"
};
static const char *const os_rotulos[OS_STATUS_TOTAL] = {
    "Aberta", "Em diagnóstico", "Aguardando aprovação", "Aprovada",
    "Aguardando peça", "Em reparo", "Concluída", "Entregue", "Cancelada"
};
#define OS_SQL_BASE \
    "SELECT o.id, o.equipamento_id, c.id, c.nome, e.tipo, e.marca, " \
    "e.modelo, e.numero_serie, COALESCE(t.id, 0), COALESCE(t.nome, ''), " \
    "o.descricao_problema, o.diagnostico, o.status, o.data_abertura " \
    "FROM ordens_servico o JOIN equipamentos e ON e.id = o.equipamento_id " \
    "JOIN clientes c ON c.id = e.cliente_id " \
    "LEFT JOIN tecnicos t ON t.id = o.tecnico_id "

static const char os_sql_id[] = OS_SQL_BASE "WHERE o.id = ?1;";
static const char os_sql_todas[] = OS_SQL_BASE "ORDER BY o.id;";
static const char os_sql_status[] =
    OS_SQL_BASE "WHERE o.status = ?1 ORDER BY o.id;";
static const int transicoes[OS_STATUS_TOTAL][OS_STATUS_TOTAL] = {
    [OS_ABERTA] = {
        [OS_EM_DIAGNOSTICO] = 1, [OS_CANCELADA] = 1
    },
    [OS_EM_DIAGNOSTICO] = {
        [OS_AGUARDANDO_APROVACAO] = 1, [OS_CANCELADA] = 1
    },
    [OS_AGUARDANDO_APROVACAO] = {
        [OS_APROVADA] = 1, [OS_CANCELADA] = 1
    },
    [OS_APROVADA] = {
        [OS_AGUARDANDO_PECA] = 1, [OS_EM_REPARO] = 1, [OS_CANCELADA] = 1
    },
    [OS_AGUARDANDO_PECA] = {
        [OS_EM_REPARO] = 1, [OS_CANCELADA] = 1
    },
    [OS_EM_REPARO] = {
        [OS_AGUARDANDO_PECA] = 1, [OS_CONCLUIDA] = 1, [OS_CANCELADA] = 1
    },
    [OS_CONCLUIDA] = {[OS_ENTREGUE] = 1}
};

const char *os_status_codigo(StatusOS status)
{
    if (status < OS_ABERTA || status >= OS_STATUS_TOTAL) {
        return NULL;
    }
    return os_codigos[status];
}

const char *os_status_rotulo(StatusOS status)
{
    if (status < OS_ABERTA || status >= OS_STATUS_TOTAL) {
        return NULL;
    }
    return os_rotulos[status];
}

int os_status_de_codigo(const char *codigo)
{
    int i;

    if (codigo == NULL) {
        return -1;
    }
    for (i = 0; i < OS_STATUS_TOTAL; i++) {
        if (strcmp(codigo, os_codigos[i]) == 0) {
            return i;
        }
    }
    return -1;
}

int ordem_transicao_permitida(StatusOS atual, StatusOS novo_status)
{
    if (atual < OS_ABERTA || atual >= OS_STATUS_TOTAL ||
        novo_status < OS_ABERTA || novo_status >= OS_STATUS_TOTAL) {
        return 0;
    }
    return transicoes[atual][novo_status];
}

int ordem_encerrada(const char *status_codigo)
{
    return status_codigo != NULL &&
        (strcmp(status_codigo, "CONCLUIDA") == 0 ||
         strcmp(status_codigo, "ENTREGUE") == 0 ||
         strcmp(status_codigo, "CANCELADA") == 0);
}

static void ordem_ler_colunas(sqlite3_stmt *consulta, OrdemServico *ordem)
{
    ordem->id = sqlite3_column_int(consulta, 0);
    ordem->equipamento_id = sqlite3_column_int(consulta, 1);
    ordem->cliente_id = sqlite3_column_int(consulta, 2);
    db_copiar_coluna(ordem->cliente_nome, sizeof(ordem->cliente_nome),
                     consulta, 3);
    db_copiar_coluna(ordem->equip_tipo, sizeof(ordem->equip_tipo), consulta, 4);
    db_copiar_coluna(ordem->equip_marca, sizeof(ordem->equip_marca),
                     consulta, 5);
    db_copiar_coluna(ordem->equip_modelo, sizeof(ordem->equip_modelo),
                     consulta, 6);
    db_copiar_coluna(ordem->equip_serie, sizeof(ordem->equip_serie),
                     consulta, 7);
    ordem->tecnico_id = sqlite3_column_int(consulta, 8);
    db_copiar_coluna(ordem->tecnico_nome, sizeof(ordem->tecnico_nome),
                     consulta, 9);
    db_copiar_coluna(ordem->descricao_problema,
                     sizeof(ordem->descricao_problema), consulta, 10);
    db_copiar_coluna(ordem->diagnostico, sizeof(ordem->diagnostico), consulta,
                     11);
    db_copiar_coluna(ordem->status, sizeof(ordem->status), consulta, 12);
    db_copiar_coluna(ordem->data_abertura, sizeof(ordem->data_abertura),
                     consulta, 13);
}

static Resultado ordem_inserir(sqlite3 *db, int equipamento_id, int tecnico_id,
                               const char *descricao, int *id_novo)
{
    static const char sql[] =
        "INSERT INTO ordens_servico "
        "(equipamento_id, tecnico_id, descricao_problema, status) "
        "VALUES (?1, ?2, ?3, 'ABERTA');";
    sqlite3_stmt *consulta = NULL;
    int codigo;
    Resultado resultado;

    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    codigo = sqlite3_bind_int(consulta, 1, equipamento_id);
    if (codigo == SQLITE_OK) {
        codigo = tecnico_id == 0 ? sqlite3_bind_null(consulta, 2)
                                 : sqlite3_bind_int(consulta, 2, tecnico_id);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 3, descricao, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    resultado = codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
    if (resultado == RES_OK) {
        *id_novo = (int)sqlite3_last_insert_rowid(db);
    }
    return db_finalizar(db, consulta, resultado);
}

static Resultado ordem_registrar_abertura(sqlite3 *db, int ordem_id,
                                          int tecnico_id,
                                          const Tecnico *tecnico)
{
    char descricao[160];
    Resultado resultado = historico_registrar(db, ordem_id, "ABERTURA", NULL,
                                                "ABERTA", "OS aberta");

    if (resultado != RES_OK || tecnico_id == 0) {
        return resultado;
    }
    snprintf(descricao, sizeof(descricao), "Técnico atribuído: %s",
             tecnico->nome);
    return historico_registrar(db, ordem_id, "TECNICO", NULL, NULL, descricao);
}

static Resultado ordem_reverter(sqlite3 *db, Resultado erro)
{
    (void)db_desfazer(db);
    return erro;
}

static Resultado ordem_validar_referencias(sqlite3 *db, int equipamento_id,
                                           int tecnico_id, Tecnico *tecnico)
{
    Equipamento equipamento;
    Resultado resultado = equipamento_buscar_por_id(db, equipamento_id,
                                                      &equipamento);

    if (resultado == RES_OK && tecnico_id > 0) {
        resultado = tecnico_buscar_por_id(db, tecnico_id, tecnico);
    }
    return resultado;
}

Resultado ordem_abrir(sqlite3 *db, int equipamento_id, int tecnico_id,
                      const char *descricao_problema, int *id_novo)
{
    Tecnico tecnico = {0};
    Resultado resultado;

    if (id_novo != NULL) {
        *id_novo = 0;
    }
    if (db == NULL || id_novo == NULL || equipamento_id <= 0 ||
        tecnico_id < 0 || descricao_problema == NULL ||
        descricao_problema[0] == '\0') {
        return RES_INVALIDO;
    }
    resultado = ordem_validar_referencias(db, equipamento_id, tecnico_id,
                                           &tecnico);
    if (resultado != RES_OK) return resultado;
    resultado = db_iniciar_transacao(db);
    if (resultado != RES_OK) {
        return resultado;
    }
    resultado = ordem_inserir(db, equipamento_id, tecnico_id,
                              descricao_problema, id_novo);
    if (resultado == RES_OK) {
        resultado = ordem_registrar_abertura(db, *id_novo, tecnico_id, &tecnico);
    }
    if (resultado != RES_OK) {
        *id_novo = 0;
        return ordem_reverter(db, resultado);
    }
    resultado = db_confirmar(db);
    if (resultado != RES_OK) {
        *id_novo = 0;
        return ordem_reverter(db, resultado);
    }
    return RES_OK;
}

Resultado ordem_buscar_por_id(sqlite3 *db, int id, OrdemServico *saida)
{
    sqlite3_stmt *consulta = NULL;
    int codigo;

    if (db == NULL || saida == NULL || id <= 0) {
        return RES_INVALIDO;
    }
    if (sqlite3_prepare_v2(db, os_sql_id, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    codigo = sqlite3_bind_int(consulta, 1, id);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    if (codigo == SQLITE_ROW) {
        ordem_ler_colunas(consulta, saida);
        return db_finalizar(db, consulta, RES_OK);
    }
    if (codigo == SQLITE_DONE) {
        return db_finalizar(db, consulta, RES_NAO_ENCONTRADO);
    }
    return db_finalizar(db, consulta, db_traduzir_erro(db));
}

static Resultado ordem_adicionar_linha(sqlite3_stmt *consulta,
                                       OrdemServico **ordens,
                                       size_t *capacidade, int quantidade)
{
    if ((size_t)quantidade == *capacidade) {
        OrdemServico *temporario = util_dobrar_capacidade(
            *ordens, capacidade, sizeof(**ordens));
        if (temporario == NULL) {
            return RES_ERRO_MEMORIA;
        }
        *ordens = temporario;
    }
    ordem_ler_colunas(consulta, &(*ordens)[quantidade]);
    return RES_OK;
}

static Resultado ordem_ler_linhas(sqlite3 *db, sqlite3_stmt *consulta,
                                  OrdemServico **ordens, size_t *capacidade,
                                  int *total)
{
    int codigo;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        if (*total == INT_MAX) {
            return RES_ERRO_MEMORIA;
        }
        {
            Resultado resultado = ordem_adicionar_linha(
                consulta, ordens, capacidade, *total);
            if (resultado != RES_OK) {
                return resultado;
            }
        }
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado ordem_listar_preparar(sqlite3 *db, const char *status_codigo,
                                       sqlite3_stmt **consulta)
{
    const char *sql = status_codigo == NULL ? os_sql_todas : os_sql_status;
    int codigo = sqlite3_prepare_v2(db, sql, -1, consulta, NULL);

    if (codigo != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    if (status_codigo != NULL &&
        db_vincular_texto(*consulta, 1, status_codigo, 0) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    return RES_OK;
}

static Resultado ordem_publicar_lista(OrdemServico *ordens, int total,
                                      OrdemServico **lista, int *quantidade)
{
    if (total == 0) {
        free(ordens);
        return RES_OK;
    }
    *lista = ordens;
    *quantidade = total;
    return RES_OK;
}

Resultado ordem_listar(sqlite3 *db, const char *status_codigo,
                       OrdemServico **lista, int *quantidade)
{
    sqlite3_stmt *consulta = NULL;
    OrdemServico *ordens;
    size_t capacidade = 8;
    int total = 0;
    Resultado resultado;

    if (db == NULL || lista == NULL || quantidade == NULL ||
        (status_codigo != NULL && os_status_de_codigo(status_codigo) < 0)) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;
    ordens = malloc(capacidade * sizeof(*ordens));
    if (ordens == NULL) {
        return RES_ERRO_MEMORIA;
    }
    resultado = ordem_listar_preparar(db, status_codigo, &consulta);
    if (resultado == RES_OK) {
        resultado = ordem_ler_linhas(db, consulta, &ordens, &capacidade, &total);
    }
    if (consulta != NULL) {
        resultado = db_finalizar(db, consulta, resultado);
    }
    if (resultado != RES_OK) {
        free(ordens);
        return resultado;
    }
    return ordem_publicar_lista(ordens, total, lista, quantidade);
}

static Resultado ordem_iniciar_e_buscar(sqlite3 *db, int ordem_id,
                                        OrdemServico *ordem)
{
    Resultado resultado = db_iniciar_transacao(db);

    if (resultado != RES_OK) {
        return resultado;
    }
    resultado = ordem_buscar_por_id(db, ordem_id, ordem);
    return resultado == RES_OK ? RES_OK : ordem_reverter(db, resultado);
}

static Resultado ordem_executar_update_texto(sqlite3 *db, const char *sql,
                                            int ordem_id, const char *texto)
{
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 1, texto, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_bind_int(consulta, 2, ordem_id);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    return db_finalizar(db, consulta,
                        codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db));
}

static Resultado ordem_executar_update_tecnico(sqlite3 *db, int ordem_id,
                                               int tecnico_id)
{
    static const char sql[] =
        "UPDATE ordens_servico SET tecnico_id = ?1 WHERE id = ?2;";
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 1, tecnico_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 2, ordem_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    return db_finalizar(db, consulta,
                        codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db));
}

static Resultado ordem_finalizar_transacao(sqlite3 *db, Resultado resultado)
{
    if (resultado != RES_OK) {
        return ordem_reverter(db, resultado);
    }
    resultado = db_confirmar(db);
    return resultado == RES_OK ? RES_OK : ordem_reverter(db, resultado);
}

Resultado ordem_atribuir_tecnico(sqlite3 *db, int ordem_id, int tecnico_id)
{
    OrdemServico ordem;
    Tecnico tecnico = {0};
    Tecnico antigo = {0};
    char descricao[501];
    Resultado resultado;

    if (db == NULL || ordem_id <= 0 || tecnico_id <= 0) return RES_INVALIDO;
    resultado = ordem_iniciar_e_buscar(db, ordem_id, &ordem);
    if (resultado != RES_OK) return resultado;
    resultado = tecnico_buscar_por_id(db, tecnico_id, &tecnico);
    if (resultado != RES_OK) return ordem_reverter(db, resultado);
    if (ordem_encerrada(ordem.status) || ordem.tecnico_id == tecnico_id) {
        return ordem_reverter(db, RES_REGRA_NEGOCIO);
    }
    if (ordem.tecnico_id > 0) {
        resultado = tecnico_buscar_por_id(db, ordem.tecnico_id, &antigo);
        if (resultado != RES_OK) return ordem_reverter(db, resultado);
    }
    if (ordem.tecnico_id == 0) {
        snprintf(descricao, sizeof(descricao), "Técnico atribuído: %s",
                 tecnico.nome);
    } else {
        snprintf(descricao, sizeof(descricao), "Técnico alterado: %s → %s",
                 antigo.nome, tecnico.nome);
    }
    resultado = ordem_executar_update_tecnico(db, ordem_id, tecnico_id);
    if (resultado == RES_OK) {
        resultado = historico_registrar(db, ordem_id, "TECNICO", NULL, NULL,
                                         descricao);
    }
    return ordem_finalizar_transacao(db, resultado);
}

Resultado ordem_registrar_diagnostico(sqlite3 *db, int ordem_id,
                                      const char *diagnostico)
{
    static const char sql[] =
        "UPDATE ordens_servico SET diagnostico = ?1 WHERE id = ?2;";
    OrdemServico ordem;
    Resultado resultado;

    if (db == NULL || ordem_id <= 0) return RES_INVALIDO;
    resultado = ordem_iniciar_e_buscar(db, ordem_id, &ordem);
    if (resultado != RES_OK) return resultado;
    if (diagnostico == NULL || diagnostico[0] == '\0' ||
        strlen(diagnostico) > 500) {
        return ordem_reverter(db, RES_INVALIDO);
    }
    if (strcmp(ordem.status, "ABERTA") == 0 || ordem_encerrada(ordem.status)) {
        return ordem_reverter(db, RES_REGRA_NEGOCIO);
    }
    resultado = ordem_executar_update_texto(db, sql, ordem_id, diagnostico);
    if (resultado == RES_OK) {
        resultado = historico_registrar(db, ordem_id, "DIAGNOSTICO", NULL,
                                         NULL, diagnostico);
    }
    return ordem_finalizar_transacao(db, resultado);
}

Resultado ordem_registrar_observacao(sqlite3 *db, int ordem_id,
                                     const char *observacao)
{
    OrdemServico ordem;
    Resultado resultado;

    if (db == NULL || ordem_id <= 0 || observacao == NULL ||
        observacao[0] == '\0' || strlen(observacao) > 500) {
        return RES_INVALIDO;
    }
    resultado = ordem_buscar_por_id(db, ordem_id, &ordem);
    if (resultado != RES_OK) return resultado;
    return historico_registrar(db, ordem_id, "OBSERVACAO", NULL, NULL,
                               observacao);
}

Resultado ordem_atualizar_status(sqlite3 *db, int ordem_id,
                                 StatusOS novo_status,
                                 const char *observacao)
{
    static const char sql[] =
        "UPDATE ordens_servico SET status = ?1 WHERE id = ?2;";
    OrdemServico ordem;
    StatusOS atual;
    Resultado resultado;

    if (db == NULL || ordem_id <= 0) return RES_INVALIDO;
    resultado = ordem_iniciar_e_buscar(db, ordem_id, &ordem);
    if (resultado != RES_OK) return resultado;
    atual = (StatusOS)os_status_de_codigo(ordem.status);
    if (!ordem_transicao_permitida(atual, novo_status)) {
        return ordem_reverter(db, RES_REGRA_NEGOCIO);
    }
    if (novo_status == OS_EM_DIAGNOSTICO && ordem.tecnico_id == 0) {
        return ordem_reverter(db, RES_REGRA_NEGOCIO);
    }
    if (novo_status == OS_CONCLUIDA && ordem.diagnostico[0] == '\0') {
        return ordem_reverter(db, RES_REGRA_NEGOCIO);
    }
    if (observacao != NULL && strlen(observacao) > 500) {
        return ordem_reverter(db, RES_INVALIDO);
    }
    if (novo_status == OS_CANCELADA &&
        (observacao == NULL || observacao[0] == '\0')) {
        return ordem_reverter(db, RES_INVALIDO);
    }
    resultado = ordem_executar_update_texto(db, sql, ordem_id,
                                            os_status_codigo(novo_status));
    if (resultado == RES_OK) {
        resultado = historico_registrar(db, ordem_id, "STATUS",
                                         ordem.status,
                                         os_status_codigo(novo_status),
                                         observacao);
    }
    return ordem_finalizar_transacao(db, resultado);
}
