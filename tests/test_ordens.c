#include "clientes.h"
#include "equipamentos.h"
#include "historico.h"
#include "ordens.h"
#include "tecnicos.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_ordens.sqlite"

static int criar_dados(sqlite3 *db, int *equipamento_id, int *tecnico_id)
{
    Cliente cliente = {0};
    Equipamento equipamento = {0};
    Tecnico tecnico = {0};
    int cliente_id;

    snprintf(cliente.nome, sizeof(cliente.nome), "%s", "Cliente OS");
    snprintf(cliente.documento, sizeof(cliente.documento), "%s", "12345678901");
    snprintf(cliente.telefone, sizeof(cliente.telefone), "%s", "11999999999");
    if (cliente_inserir(db, &cliente, &cliente_id) != RES_OK) return 0;
    equipamento.cliente_id = cliente_id;
    snprintf(equipamento.tipo, sizeof(equipamento.tipo), "%s", "NOTEBOOK");
    snprintf(equipamento.marca, sizeof(equipamento.marca), "%s", "Marca OS");
    snprintf(equipamento.modelo, sizeof(equipamento.modelo), "%s", "Modelo OS");
    snprintf(equipamento.numero_serie, sizeof(equipamento.numero_serie), "%s",
             "OS-SERIE-1");
    if (equipamento_inserir(db, &equipamento, equipamento_id) != RES_OK) return 0;
    snprintf(tecnico.nome, sizeof(tecnico.nome), "%s", "Tecnico OS");
    snprintf(tecnico.especialidade, sizeof(tecnico.especialidade), "%s",
             "Hardware");
    return tecnico_inserir(db, &tecnico, tecnico_id) == RES_OK;
}

static void testar_status(void)
{
    static const char *const codigos[] = {
        "ABERTA", "EM_DIAGNOSTICO", "AGUARDANDO_APROVACAO", "APROVADA",
        "AGUARDANDO_PECA", "EM_REPARO", "CONCLUIDA", "ENTREGUE", "CANCELADA"
    };
    static const char *const rotulos[] = {
        "Aberta", "Em diagnóstico", "Aguardando aprovação", "Aprovada",
        "Aguardando peça", "Em reparo", "Concluída", "Entregue", "Cancelada"
    };
    int i;

    for (i = 0; i < OS_STATUS_TOTAL; i++) {
        TESTE_VERIFICAR(strcmp(os_status_codigo((StatusOS)i), codigos[i]) == 0,
                        "código de situação");
        TESTE_VERIFICAR(strcmp(os_status_rotulo((StatusOS)i), rotulos[i]) == 0,
                        "rótulo de situação");
        TESTE_VERIFICAR(os_status_de_codigo(codigos[i]) == i,
                        "converter código de situação");
    }
    TESTE_VERIFICAR(os_status_de_codigo("EM_REPARO") == OS_EM_REPARO,
                    "buscar código EM_REPARO");
    TESTE_VERIFICAR(os_status_de_codigo("DESCONHECIDO") == -1,
                    "rejeitar código de situação desconhecido");
}

static int contar_tabela(sqlite3 *db, const char *tabela)
{
    char sql[96];
    sqlite3_stmt *consulta = NULL;
    int total = -1;

    if (snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s;", tabela) < 0) {
        return -1;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) == SQLITE_OK &&
        sqlite3_step(consulta) == SQLITE_ROW) {
        total = sqlite3_column_int(consulta, 0);
    }
    if (sqlite3_finalize(consulta) != SQLITE_OK) return -1;
    return total;
}

static int criar_os_sem_historico(sqlite3 *db, int equipamento_id, int *id)
{
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(
        db, "INSERT INTO ordens_servico "
        "(equipamento_id, descricao_problema, status) "
        "VALUES (?1, 'OS sem histórico', 'CANCELADA');",
        -1, &consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 1, equipamento_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (codigo == SQLITE_DONE) *id = (int)sqlite3_last_insert_rowid(db);
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) return 0;
    return codigo == SQLITE_DONE;
}

static void testar_historico_vazio(sqlite3 *db, int ordem_id)
{
    EventoHistorico *lista = NULL;
    int quantidade = -1;
    Resultado resultado = historico_listar_por_ordem(db, ordem_id, &lista,
                                                       &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 0 && lista == NULL,
                    "listar histórico vazio");
    free(lista);
}

static void testar_falhas_sem_meio_registro(sqlite3 *db, int equipamento_id,
                                            int ordem_id)
{
    int ordens_antes = contar_tabela(db, "ordens_servico");
    int eventos_antes = contar_tabela(db, "historico_ordens");
    int id_novo = 0;

    TESTE_VERIFICAR(ordem_abrir(db, 999999, 0, "Problema", &id_novo) ==
                        RES_NAO_ENCONTRADO,
                    "rejeitar equipamento inexistente");
    TESTE_VERIFICAR(contar_tabela(db, "ordens_servico") == ordens_antes &&
                    contar_tabela(db, "historico_ordens") == eventos_antes,
                    "falha de equipamento não grava parcialmente");
    TESTE_VERIFICAR(ordem_abrir(db, equipamento_id, 999999, "Problema",
                                &id_novo) == RES_NAO_ENCONTRADO,
                    "rejeitar técnico inexistente");
    TESTE_VERIFICAR(contar_tabela(db, "ordens_servico") == ordens_antes &&
                    contar_tabela(db, "historico_ordens") == eventos_antes,
                    "falha de técnico não grava parcialmente");
    TESTE_VERIFICAR(ordem_abrir(db, equipamento_id, 0, "", &id_novo) ==
                        RES_INVALIDO,
                    "rejeitar descrição vazia");
    TESTE_VERIFICAR(contar_tabela(db, "ordens_servico") == ordens_antes &&
                    contar_tabela(db, "historico_ordens") == eventos_antes,
                    "descrição inválida não grava parcialmente");
    TESTE_VERIFICAR(ordem_buscar_por_id(db, ordem_id, &(OrdemServico){0}) ==
                        RES_OK,
                    "OS inicial permanece disponível");
}

static void testar_historico_dois_eventos(sqlite3 *db, int ordem_id)
{
    EventoHistorico *lista = NULL;
    int quantidade = 0;
    Resultado resultado = historico_listar_por_ordem(db, ordem_id, &lista,
                                                       &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 2,
                    "ordem com técnico tem dois eventos");
    if (resultado == RES_OK && quantidade == 2) {
        TESTE_VERIFICAR(strcmp(lista[0].tipo_evento, "ABERTURA") == 0,
                        "primeiro evento é abertura");
        TESTE_VERIFICAR(strcmp(lista[1].tipo_evento, "TECNICO") == 0,
                        "segundo evento é técnico");
    }
    free(lista);
}

static void testar_listagens(sqlite3 *db, int id_sem_evento,
                             int primeiro_id, int segundo_id,
                             int id_apos_rollback)
{
    OrdemServico *lista = NULL;
    int quantidade = 0;
    Resultado resultado = ordem_listar(db, NULL, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 4 &&
                    lista[0].id == id_sem_evento &&
                    lista[1].id == primeiro_id &&
                    lista[2].id == segundo_id &&
                    lista[3].id == id_apos_rollback,
                    "listar todas as ordens por id");
    free(lista);
    lista = NULL;
    resultado = ordem_listar(db, "ABERTA", &lista, &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 3,
                    "listar ordens abertas");
    free(lista);
    lista = NULL;
    resultado = ordem_listar(db, "ENTREGUE", &lista, &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 0 && lista == NULL,
                    "listar situação válida sem ordens");
    TESTE_VERIFICAR(ordem_listar(db, "INVALIDO", &lista, &quantidade) ==
                        RES_INVALIDO,
                    "rejeitar situação desconhecida");
    free(lista);
}

static int preparar_banco(sqlite3 **db, int *equipamento_id, int *tecnico_id,
                          int *ordem_vazia)
{
    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        perror(CAMINHO_BANCO);
        TESTE_VERIFICAR(0, "apagar banco de ordens anterior");
        return 0;
    }
    {
        Resultado resultado = db_abrir(db, CAMINHO_BANCO);
        TESTE_VERIFICAR(resultado == RES_OK, "abrir banco de ordens");
        if (resultado != RES_OK) return 0;
    }
    if (!criar_dados(*db, equipamento_id, tecnico_id)) {
        TESTE_VERIFICAR(0, "criar cliente, equipamento e técnico");
        return 0;
    }
    TESTE_VERIFICAR(1, "criar cliente, equipamento e técnico");
    if (!criar_os_sem_historico(*db, *equipamento_id, ordem_vazia)) {
        TESTE_VERIFICAR(0, "criar OS sem histórico");
        return 0;
    }
    TESTE_VERIFICAR(1, "criar OS sem histórico");
    testar_historico_vazio(*db, *ordem_vazia);
    return 1;
}

static void testar_ordem_sem_tecnico(sqlite3 *db, int equipamento_id,
                                     int *ordem_id)
{
    EventoHistorico *eventos = NULL;
    OrdemServico ordem;
    int quantidade = 0;
    Resultado resultado = ordem_abrir(db, equipamento_id, 0, "Não liga",
                                      ordem_id);

    TESTE_VERIFICAR(resultado == RES_OK && *ordem_id > 0,
                    "abrir ordem sem técnico");
    resultado = ordem_buscar_por_id(db, *ordem_id, &ordem);
    TESTE_VERIFICAR(resultado == RES_OK &&
                    strcmp(ordem.status, "ABERTA") == 0 &&
                    strcmp(ordem.cliente_nome, "Cliente OS") == 0 &&
                    strcmp(ordem.equip_marca, "Marca OS") == 0 &&
                    strcmp(ordem.equip_modelo, "Modelo OS") == 0 &&
                    ordem.tecnico_id == 0 && ordem.tecnico_nome[0] == '\0',
                    "buscar dados unidos da ordem");
    resultado = historico_listar_por_ordem(db, *ordem_id, &eventos,
                                            &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 1 &&
                    strcmp(eventos[0].tipo_evento, "ABERTURA") == 0 &&
                    strcmp(eventos[0].status_novo, "ABERTA") == 0,
                    "abertura registra evento");
    free(eventos);
}

static void testar_ordem_com_tecnico(sqlite3 *db, int equipamento_id,
                                     int tecnico_id, int *ordem_id)
{
    Resultado resultado = ordem_abrir(db, equipamento_id, tecnico_id,
                                      "Tela quebrada", ordem_id);

    TESTE_VERIFICAR(resultado == RES_OK && *ordem_id > 0,
                    "abrir ordem com técnico");
    testar_historico_dois_eventos(db, *ordem_id);
}

static void testar_regras_de_ordem(sqlite3 *db, int equipamento_id,
                                   int id_ordem)
{
    OrdemServico ordem;

    testar_falhas_sem_meio_registro(db, equipamento_id, id_ordem);
    TESTE_VERIFICAR(ordem_buscar_por_id(db, 999999, &ordem) ==
                        RES_NAO_ENCONTRADO,
                    "buscar ordem inexistente");
    TESTE_VERIFICAR(historico_registrar(db, id_ordem, "INVALIDO", NULL, NULL,
                                        NULL) == RES_INVALIDO,
                    "rejeitar tipo de histórico desconhecido");
    TESTE_VERIFICAR(historico_registrar(db, 999999, "STATUS", NULL, NULL,
                                        NULL) == RES_INVALIDO,
                    "rejeitar histórico de ordem inexistente");
}

static void testar_rollback_na_transacao(sqlite3 *db, int equipamento_id,
                                         int tecnico_id, int *ordem_id)
{
    const char *criar_trigger =
        "CREATE TRIGGER falha_tecnico BEFORE INSERT ON historico_ordens "
        "WHEN NEW.tipo_evento = 'TECNICO' "
        "BEGIN SELECT RAISE(ABORT, 'falha forcada'); END;";
    int ordens_antes = contar_tabela(db, "ordens_servico");
    int eventos_antes = contar_tabela(db, "historico_ordens");
    int codigo = sqlite3_exec(db, criar_trigger, NULL, NULL, NULL);

    TESTE_VERIFICAR(codigo == SQLITE_OK, "criar trigger de falha");
    if (codigo != SQLITE_OK) return;
    TESTE_VERIFICAR(ordem_abrir(db, equipamento_id, tecnico_id,
                                "Falha simulada", ordem_id) != RES_OK,
                    "falha no evento TECNICO rejeita abertura");
    TESTE_VERIFICAR(contar_tabela(db, "ordens_servico") == ordens_antes &&
                    contar_tabela(db, "historico_ordens") == eventos_antes,
                    "rollback remove OS e evento ABERTURA");
    codigo = sqlite3_exec(db, "DROP TRIGGER falha_tecnico;",
                          NULL, NULL, NULL);
    TESTE_VERIFICAR(codigo == SQLITE_OK, "remover trigger de falha");
    if (codigo != SQLITE_OK) return;
    TESTE_VERIFICAR(ordem_abrir(db, equipamento_id, tecnico_id,
                                "Após rollback", ordem_id) == RES_OK,
                    "nova abertura funciona após rollback");
}

static void testar_persistencia(sqlite3 **db, int ordem_id)
{
    EventoHistorico *eventos = NULL;
    OrdemServico ordem;
    int quantidade = 0;
    Resultado resultado;

    db_fechar(*db);
    *db = NULL;
    resultado = db_abrir(db, CAMINHO_BANCO);
    TESTE_VERIFICAR(resultado == RES_OK, "reabrir banco de ordens");
    if (resultado != RES_OK) return;
    TESTE_VERIFICAR(ordem_buscar_por_id(*db, ordem_id, &ordem) == RES_OK &&
                    strcmp(ordem.status, "ABERTA") == 0,
                    "ordem persiste após reabrir");
    resultado = historico_listar_por_ordem(*db, ordem_id, &eventos,
                                            &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 1,
                    "histórico persiste após reabrir");
    free(eventos);
}

int main(void)
{
    sqlite3 *db = NULL;
    int equipamento_id = 0;
    int tecnico_id = 0;
    int ordem_vazia = 0;
    int ordem_sem_tecnico = 0;
    int ordem_com_tecnico = 0;
    int ordem_apos_rollback = 0;

    testar_status();
    if (!preparar_banco(&db, &equipamento_id, &tecnico_id, &ordem_vazia)) {
        db_fechar(db);
        return teste_codigo_saida();
    }
    testar_ordem_sem_tecnico(db, equipamento_id, &ordem_sem_tecnico);
    testar_ordem_com_tecnico(db, equipamento_id, tecnico_id,
                             &ordem_com_tecnico);
    testar_regras_de_ordem(db, equipamento_id, ordem_sem_tecnico);
    testar_rollback_na_transacao(db, equipamento_id, tecnico_id,
                                 &ordem_apos_rollback);
    testar_listagens(db, ordem_vazia, ordem_sem_tecnico, ordem_com_tecnico,
                     ordem_apos_rollback);
    testar_persistencia(&db, ordem_sem_tecnico);
    db_fechar(db);
    return teste_codigo_saida();
}
