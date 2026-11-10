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

#define CAMINHO_BANCO "build\\test_ordens_fluxo.sqlite"

static int criar_dados(sqlite3 *db, int *equipamento_id, int *tecnico_a,
                       int *tecnico_b)
{
    Cliente cliente = {0};
    Equipamento equipamento = {0};
    Tecnico tecnico = {0};
    int cliente_id;

    snprintf(cliente.nome, sizeof(cliente.nome), "%s", "Cliente Fluxo");
    snprintf(cliente.documento, sizeof(cliente.documento), "%s",
             "12345678901");
    snprintf(cliente.telefone, sizeof(cliente.telefone), "%s", "11999999999");
    if (cliente_inserir(db, &cliente, &cliente_id) != RES_OK) return 0;
    equipamento.cliente_id = cliente_id;
    snprintf(equipamento.tipo, sizeof(equipamento.tipo), "%s", "NOTEBOOK");
    snprintf(equipamento.marca, sizeof(equipamento.marca), "%s", "Marca");
    snprintf(equipamento.modelo, sizeof(equipamento.modelo), "%s", "Modelo");
    snprintf(equipamento.numero_serie, sizeof(equipamento.numero_serie), "%s",
             "FLUXO-1");
    if (equipamento_inserir(db, &equipamento, equipamento_id) != RES_OK) {
        return 0;
    }
    snprintf(tecnico.nome, sizeof(tecnico.nome), "%s", "Tecnico A");
    snprintf(tecnico.especialidade, sizeof(tecnico.especialidade), "%s",
             "Hardware");
    if (tecnico_inserir(db, &tecnico, tecnico_a) != RES_OK) return 0;
    snprintf(tecnico.nome, sizeof(tecnico.nome), "%s", "Tecnico B");
    return tecnico_inserir(db, &tecnico, tecnico_b) == RES_OK;
}

static int preparar_banco(sqlite3 **db, int *equipamento_id, int *tecnico_a,
                          int *tecnico_b)
{
    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        TESTE_VERIFICAR(0, "remover banco anterior");
        return 0;
    }
    if (db_abrir(db, CAMINHO_BANCO) != RES_OK) {
        TESTE_VERIFICAR(0, "abrir banco de fluxo");
        return 0;
    }
    TESTE_VERIFICAR(criar_dados(*db, equipamento_id, tecnico_a, tecnico_b),
                    "criar cliente, equipamento e técnicos");
    return 1;
}

static void testar_matriz(void)
{
    int total = 0;
    int atual;
    int novo;

    for (atual = 0; atual < OS_STATUS_TOTAL; atual++) {
        for (novo = 0; novo < OS_STATUS_TOTAL; novo++) {
            total += ordem_transicao_permitida((StatusOS)atual,
                                               (StatusOS)novo);
        }
    }
    TESTE_VERIFICAR(ordem_transicao_permitida(OS_ABERTA,
                                               OS_EM_DIAGNOSTICO),
                    "ABERTA permite EM_DIAGNOSTICO");
    TESTE_VERIFICAR(!ordem_transicao_permitida(OS_ABERTA, OS_CONCLUIDA),
                    "ABERTA não permite CONCLUIDA");
    TESTE_VERIFICAR(!ordem_transicao_permitida(OS_ENTREGUE, OS_ABERTA) &&
                    !ordem_transicao_permitida(OS_CANCELADA, OS_ABERTA),
                    "ENTREGUE e CANCELADA não têm destinos");
    TESTE_VERIFICAR(ordem_transicao_permitida(OS_CONCLUIDA, OS_ENTREGUE),
                    "CONCLUIDA permite ENTREGUE");
    TESTE_VERIFICAR(total == 15, "matriz tem 15 transições");
}

static int abrir_teste(sqlite3 *db, int equipamento_id, int tecnico_id,
                       const char *descricao)
{
    int id = 0;
    Resultado resultado = ordem_abrir(db, equipamento_id, tecnico_id,
                                      descricao, &id);

    TESTE_VERIFICAR(resultado == RES_OK && id > 0, "abrir ordem de teste");
    return resultado == RES_OK ? id : 0;
}

static void testar_inicio_fluxo(sqlite3 *db, int id, int tecnico_a)
{
    TESTE_VERIFICAR(ordem_atualizar_status(db, id, OS_EM_DIAGNOSTICO, NULL) ==
                        RES_REGRA_NEGOCIO,
                    "impedir diagnóstico sem técnico");
    TESTE_VERIFICAR(ordem_atribuir_tecnico(db, id, tecnico_a) == RES_OK,
                    "atribuir técnico");
    TESTE_VERIFICAR(ordem_atualizar_status(db, id, OS_EM_DIAGNOSTICO, NULL) ==
                        RES_OK,
                    "iniciar diagnóstico com técnico");
    TESTE_VERIFICAR(ordem_registrar_diagnostico(db, id, "Placa danificada") ==
                        RES_OK,
                    "registrar diagnóstico");
}

static void testar_historico_fluxo(sqlite3 *db, int id)
{
    static const char *const anteriores[] = {
        "ABERTA", "EM_DIAGNOSTICO", "AGUARDANDO_APROVACAO", "APROVADA",
        "EM_REPARO", "CONCLUIDA"
    };
    static const char *const novos[] = {
        "EM_DIAGNOSTICO", "AGUARDANDO_APROVACAO", "APROVADA", "EM_REPARO",
        "CONCLUIDA", "ENTREGUE"
    };
    EventoHistorico *eventos = NULL;
    int quantidade = 0;
    int i;

    TESTE_VERIFICAR(historico_listar_por_ordem(db, id, &eventos,
                                                &quantidade) == RES_OK &&
                    quantidade == 9,
                    "histórico contém eventos do fluxo");
    if (quantidade != 9) {
        free(eventos);
        return;
    }
    TESTE_VERIFICAR(strcmp(eventos[0].tipo_evento, "ABERTURA") == 0 &&
                    strcmp(eventos[1].tipo_evento, "TECNICO") == 0 &&
                    strcmp(eventos[2].tipo_evento, "STATUS") == 0 &&
                    strcmp(eventos[3].tipo_evento, "DIAGNOSTICO") == 0,
                    "histórico inicia na ordem correta");
    for (i = 0; i < 6; i++) {
        int indice = i == 0 ? 2 : i + 3;
        TESTE_VERIFICAR(strcmp(eventos[indice].status_anterior,
                               anteriores[i]) == 0 &&
                        strcmp(eventos[indice].status_novo, novos[i]) == 0,
                        "histórico registra situação na ordem correta");
    }
    free(eventos);
}

static void testar_fluxo_completo(sqlite3 *db, int equipamento_id,
                                  int tecnico_a, int *ordem_final)
{
    static const StatusOS etapas[] = {
        OS_EM_DIAGNOSTICO, OS_AGUARDANDO_APROVACAO, OS_APROVADA,
        OS_EM_REPARO, OS_CONCLUIDA, OS_ENTREGUE
    };
    OrdemServico ordem;
    int id = abrir_teste(db, equipamento_id, 0, "Falha no equipamento");
    int i;

    testar_inicio_fluxo(db, id, tecnico_a);
    for (i = 1; i < (int)(sizeof(etapas) / sizeof(etapas[0])); i++) {
        TESTE_VERIFICAR(ordem_atualizar_status(db, id, etapas[i], NULL) ==
                            RES_OK,
                        "avançar etapa do fluxo");
    }
    TESTE_VERIFICAR(ordem_buscar_por_id(db, id, &ordem) == RES_OK &&
                    strcmp(ordem.status, "ENTREGUE") == 0,
                    "fluxo termina como ENTREGUE");
    testar_historico_fluxo(db, id);
    *ordem_final = id;
}

static void testar_rejeicoes_status(sqlite3 *db, int equipamento_id)
{
    OrdemServico ordem;
    EventoHistorico *eventos = NULL;
    int quantidade = 0;
    int id_aberta = abrir_teste(db, equipamento_id, 0, "Ordem aberta");

    TESTE_VERIFICAR(ordem_atualizar_status(db, id_aberta, OS_CONCLUIDA, NULL)
                        == RES_REGRA_NEGOCIO,
                    "rejeitar transição inválida");
    TESTE_VERIFICAR(ordem_buscar_por_id(db, id_aberta, &ordem) == RES_OK &&
                    strcmp(ordem.status, "ABERTA") == 0,
                    "transição inválida preserva situação");
    TESTE_VERIFICAR(historico_listar_por_ordem(db, id_aberta, &eventos,
                                                &quantidade) == RES_OK &&
                    quantidade == 1,
                    "transição inválida não grava histórico");
    free(eventos);
    TESTE_VERIFICAR(ordem_atualizar_status(db, id_aberta, OS_CANCELADA, NULL)
                        == RES_INVALIDO,
                    "cancelamento exige motivo");
}

static void testar_cancelamento(sqlite3 *db, int equipamento_id,
                                int tecnico_id)
{
    EventoHistorico *eventos = NULL;
    int quantidade = 0;
    int id_cancelada = abrir_teste(db, equipamento_id, tecnico_id, "Cancelar");

    TESTE_VERIFICAR(ordem_atualizar_status(db, id_cancelada, OS_CANCELADA,
                                           "Solicitado pelo cliente") ==
                        RES_OK,
                    "cancelar com motivo");
    TESTE_VERIFICAR(historico_listar_por_ordem(db, id_cancelada, &eventos,
                                                &quantidade) == RES_OK &&
                    quantidade == 3 &&
                    strcmp(eventos[2].descricao,
                           "Solicitado pelo cliente") == 0,
                    "motivo aparece no histórico");
    free(eventos);
}

static void testar_atribuicao_tecnico(sqlite3 *db, int equipamento_id,
                                      int tecnico_a, int tecnico_b,
                                      int ordem_final)
{
    EventoHistorico *eventos = NULL;
    int quantidade = 0;
    int id_aberta = abrir_teste(db, equipamento_id, 0, "Ordem aberta");

    TESTE_VERIFICAR(ordem_atribuir_tecnico(db, 999999, tecnico_a) ==
                        RES_NAO_ENCONTRADO &&
                    ordem_atribuir_tecnico(db, id_aberta, 999999) ==
                        RES_NAO_ENCONTRADO,
                    "rejeitar ordem ou técnico inexistente");
    TESTE_VERIFICAR(ordem_atribuir_tecnico(db, ordem_final, tecnico_a) ==
                        RES_REGRA_NEGOCIO,
                    "rejeitar técnico repetido na ordem");
    TESTE_VERIFICAR(ordem_atribuir_tecnico(db, ordem_final, tecnico_b) ==
                        RES_REGRA_NEGOCIO,
                    "rejeitar alteração em ordem encerrada");
    TESTE_VERIFICAR(ordem_atribuir_tecnico(db, id_aberta, tecnico_a) == RES_OK &&
                    ordem_atribuir_tecnico(db, id_aberta, tecnico_a) ==
                        RES_REGRA_NEGOCIO &&
                    ordem_atribuir_tecnico(db, id_aberta, tecnico_b) == RES_OK,
                    "atribuir, rejeitar repetição e trocar técnico");
    eventos = NULL;
    quantidade = 0;
    TESTE_VERIFICAR(historico_listar_por_ordem(db, id_aberta, &eventos,
                                                &quantidade) == RES_OK &&
                    quantidade == 3 &&
                    strstr(eventos[2].descricao, "alterado") != NULL,
                    "troca de técnico aparece no histórico");
    free(eventos);
}

static void testar_regras_de_atualizacao(sqlite3 *db, int equipamento_id,
                                         int tecnico_a, int tecnico_b,
                                         int ordem_final)
{
    testar_rejeicoes_status(db, equipamento_id);
    testar_cancelamento(db, equipamento_id, tecnico_a);
    testar_atribuicao_tecnico(db, equipamento_id, tecnico_a, tecnico_b,
                              ordem_final);
}

static void testar_diagnostico_observacao(sqlite3 *db, int equipamento_id,
                                          int ordem_final)
{
    int id_aberta = abrir_teste(db, equipamento_id, 0, "Sem diagnóstico");
    int id_reparo = abrir_teste(db, equipamento_id, 1, "Sem diagnóstico");

    TESTE_VERIFICAR(ordem_registrar_diagnostico(db, id_aberta, "texto") ==
                        RES_REGRA_NEGOCIO,
                    "diagnóstico não permitido em ABERTA");
    TESTE_VERIFICAR(ordem_registrar_diagnostico(db, id_aberta, "") ==
                        RES_INVALIDO,
                    "diagnóstico vazio inválido");
    TESTE_VERIFICAR(ordem_registrar_diagnostico(db, ordem_final, "Novo") ==
                        RES_REGRA_NEGOCIO,
                    "diagnóstico não permitido em ordem encerrada");
    TESTE_VERIFICAR(ordem_registrar_observacao(db, ordem_final,
                                               "Observação após entrega") ==
                        RES_OK,
                    "observação permitida em ordem encerrada");
    TESTE_VERIFICAR(ordem_registrar_observacao(db, id_aberta,
                                               "Observação em aberto") ==
                        RES_OK,
                    "observação permitida em ordem aberta");
    TESTE_VERIFICAR(ordem_registrar_observacao(db, 999999, "Observação") ==
                        RES_NAO_ENCONTRADO &&
                    ordem_registrar_observacao(db, id_aberta, "") ==
                        RES_INVALIDO,
                    "validar ordem e observação");
    TESTE_VERIFICAR(ordem_atualizar_status(db, id_reparo,
                                           OS_EM_DIAGNOSTICO, NULL) == RES_OK &&
                    ordem_registrar_diagnostico(db, id_reparo, "Falha") ==
                        RES_OK,
                    "registrar diagnóstico após transição");
}

static void testar_conclusao_e_rollback(sqlite3 *db, int equipamento_id,
                                        int tecnico_id)
{
    int id = abrir_teste(db, equipamento_id, tecnico_id, "Sem diagnóstico");
    int codigo;
    OrdemServico ordem;
    const char *trigger =
        "CREATE TRIGGER falha_status BEFORE INSERT ON historico_ordens "
        "WHEN NEW.tipo_evento = 'STATUS' "
        "BEGIN SELECT RAISE(ABORT, 'falha forcada'); END;";

    TESTE_VERIFICAR(ordem_atualizar_status(db, id, OS_EM_DIAGNOSTICO, NULL) ==
                        RES_OK &&
                    ordem_atualizar_status(db, id, OS_AGUARDANDO_APROVACAO,
                                           NULL) == RES_OK &&
                    ordem_atualizar_status(db, id, OS_APROVADA, NULL) ==
                        RES_OK &&
                    ordem_atualizar_status(db, id, OS_EM_REPARO, NULL) ==
                        RES_OK,
                    "preparar ordem para validar conclusão");
    TESTE_VERIFICAR(ordem_atualizar_status(db, id, OS_CONCLUIDA, NULL) ==
                        RES_REGRA_NEGOCIO,
                    "impedir conclusão sem diagnóstico");
    TESTE_VERIFICAR(sqlite3_exec(db, trigger, NULL, NULL, NULL) == SQLITE_OK,
                    "criar trigger de rollback de situação");
    TESTE_VERIFICAR(ordem_atualizar_status(db, id, OS_AGUARDANDO_PECA, NULL)
                        != RES_OK &&
                    ordem_buscar_por_id(db, id, &ordem) == RES_OK &&
                    strcmp(ordem.status, "EM_REPARO") == 0,
                    "rollback preserva situação após falha do histórico");
    codigo = sqlite3_exec(db, "DROP TRIGGER falha_status;", NULL, NULL, NULL);
    TESTE_VERIFICAR(codigo == SQLITE_OK, "remover trigger de rollback");
    TESTE_VERIFICAR(ordem_atualizar_status(db, id, OS_AGUARDANDO_PECA, NULL)
                        == RES_OK,
                    "atualização funciona após rollback");
}

static void testar_persistencia(sqlite3 **db, int ordem_id)
{
    OrdemServico ordem;
    EventoHistorico *eventos = NULL;
    int quantidade = 0;

    db_fechar(*db);
    *db = NULL;
    TESTE_VERIFICAR(db_abrir(db, CAMINHO_BANCO) == RES_OK,
                    "reabrir banco de fluxo");
    TESTE_VERIFICAR(ordem_buscar_por_id(*db, ordem_id, &ordem) == RES_OK &&
                    strcmp(ordem.status, "ENTREGUE") == 0,
                    "situação persiste após reabrir");
    TESTE_VERIFICAR(historico_listar_por_ordem(*db, ordem_id, &eventos,
                                                &quantidade) == RES_OK &&
                    quantidade == 10,
                    "histórico persiste após reabrir");
    free(eventos);
}

int main(void)
{
    sqlite3 *db = NULL;
    int equipamento_id = 0;
    int tecnico_a = 0;
    int tecnico_b = 0;
    int ordem_final = 0;

    testar_matriz();
    if (!preparar_banco(&db, &equipamento_id, &tecnico_a, &tecnico_b)) {
        db_fechar(db);
        return teste_codigo_saida();
    }
    testar_fluxo_completo(db, equipamento_id, tecnico_a, &ordem_final);
    testar_regras_de_atualizacao(db, equipamento_id, tecnico_a, tecnico_b,
                                 ordem_final);
    testar_diagnostico_observacao(db, equipamento_id, ordem_final);
    testar_conclusao_e_rollback(db, equipamento_id, tecnico_a);
    testar_persistencia(&db, ordem_final);
    db_fechar(db);
    return teste_codigo_saida();
}
