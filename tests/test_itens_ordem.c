#include "clientes.h"
#include "equipamentos.h"
#include "historico.h"
#include "itens_ordem.h"
#include "ordens.h"
#include "pecas.h"
#include "tecnicos.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_itens_ordem.sqlite"

typedef struct Contagens {
    int estoque;
    int itens;
    int movimentos;
    int historico;
} Contagens;

typedef struct DadosTeste {
    int equipamento_id;
    int tecnico_id;
    int peca_id;
    int ordem_aberta;
    int ordem_aprovada;
} DadosTeste;

static int consultar_int(sqlite3 *db, const char *sql, int parametro)
{
    sqlite3_stmt *consulta = NULL;
    int valor = -1;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK && parametro > 0) {
        codigo = sqlite3_bind_int(consulta, 1, parametro);
    }
    if (codigo == SQLITE_OK && sqlite3_step(consulta) == SQLITE_ROW) {
        valor = sqlite3_column_int(consulta, 0);
    }
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) return -1;
    return valor;
}

static Contagens obter_contagens(sqlite3 *db, int peca_id)
{
    Contagens contagens;

    contagens.estoque = consultar_int(db,
        "SELECT quantidade FROM pecas WHERE id = ?1;", peca_id);
    contagens.itens = consultar_int(db,
        "SELECT COUNT(*) FROM itens_ordem;", 0);
    contagens.movimentos = consultar_int(db,
        "SELECT COUNT(*) FROM movimentacoes_estoque;", 0);
    contagens.historico = consultar_int(db,
        "SELECT COUNT(*) FROM historico_ordens;", 0);
    return contagens;
}

static int contagens_iguais(Contagens a, Contagens b)
{
    return a.estoque == b.estoque && a.itens == b.itens &&
           a.movimentos == b.movimentos && a.historico == b.historico;
}

static int criar_cliente_equipamento(sqlite3 *db, int *equipamento_id)
{
    Cliente cliente = {0};
    Equipamento equipamento = {0};
    int cliente_id;

    snprintf(cliente.nome, sizeof(cliente.nome), "%s", "Cliente Itens");
    snprintf(cliente.documento, sizeof(cliente.documento), "%s",
             "12345678901");
    snprintf(cliente.telefone, sizeof(cliente.telefone), "%s", "11999999999");
    if (cliente_inserir(db, &cliente, &cliente_id) != RES_OK) return 0;
    equipamento.cliente_id = cliente_id;
    snprintf(equipamento.tipo, sizeof(equipamento.tipo), "%s", "NOTEBOOK");
    snprintf(equipamento.marca, sizeof(equipamento.marca), "%s", "Marca");
    snprintf(equipamento.modelo, sizeof(equipamento.modelo), "%s", "Modelo");
    snprintf(equipamento.numero_serie, sizeof(equipamento.numero_serie), "%s",
             "ITEM-OS-1");
    if (equipamento_inserir(db, &equipamento, equipamento_id) != RES_OK) {
        return 0;
    }
    return 1;
}

static int criar_tecnico(sqlite3 *db, int *tecnico_id)
{
    Tecnico tecnico = {0};

    snprintf(tecnico.nome, sizeof(tecnico.nome), "%s", "Tecnico Itens");
    snprintf(tecnico.especialidade, sizeof(tecnico.especialidade), "%s",
             "Hardware");
    return tecnico_inserir(db, &tecnico, tecnico_id) == RES_OK;
}

static int criar_peca(sqlite3 *db, int *peca_id)
{
    Peca peca = {0};

    snprintf(peca.codigo, sizeof(peca.codigo), "%s", "MEM16");
    snprintf(peca.nome, sizeof(peca.nome), "%s", "Memória 16 GB");
    peca.quantidade = 10;
    peca.estoque_minimo = 3;
    return peca_cadastrar(db, &peca, peca_id) == RES_OK;
}

static int criar_ordem_aprovada(sqlite3 *db, int equipamento_id,
                                int tecnico_id, int *ordem_id)
{
    if (ordem_abrir(db, equipamento_id, 0, "Ordem aprovada", ordem_id) !=
        RES_OK) return 0;
    return ordem_atribuir_tecnico(db, *ordem_id, tecnico_id) == RES_OK &&
        ordem_atualizar_status(db, *ordem_id, OS_EM_DIAGNOSTICO, NULL) ==
            RES_OK &&
        ordem_registrar_diagnostico(db, *ordem_id, "Falha confirmada") ==
            RES_OK &&
        ordem_atualizar_status(db, *ordem_id, OS_AGUARDANDO_APROVACAO, NULL)
            == RES_OK &&
        ordem_atualizar_status(db, *ordem_id, OS_APROVADA, NULL) == RES_OK;
}

static int criar_dados(sqlite3 *db, DadosTeste *dados)
{
    return criar_cliente_equipamento(db, &dados->equipamento_id) &&
        criar_tecnico(db, &dados->tecnico_id) &&
        criar_peca(db, &dados->peca_id) &&
        ordem_abrir(db, dados->equipamento_id, 0, "Ordem aberta",
                    &dados->ordem_aberta) == RES_OK &&
        criar_ordem_aprovada(db, dados->equipamento_id, dados->tecnico_id,
                             &dados->ordem_aprovada);
}

static void testar_aberta_e_entradas_invalidas(sqlite3 *db, int ordem_aberta,
                                               int ordem_valida, int peca_id)
{
    Contagens antes = obter_contagens(db, peca_id);

    TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_aberta, peca_id, 1) ==
                        RES_REGRA_NEGOCIO,
                    "OS ABERTA nao permite usar peca");
    TESTE_VERIFICAR(contagens_iguais(antes, obter_contagens(db, peca_id)),
                    "uso em OS ABERTA nao altera registros");
    antes = obter_contagens(db, peca_id);
    TESTE_VERIFICAR(item_ordem_usar_peca(db, 999999, peca_id, 1) ==
                        RES_NAO_ENCONTRADO &&
                    item_ordem_usar_peca(db, ordem_valida, 999999, 1) ==
                        RES_NAO_ENCONTRADO,
                    "rejeitar OS ou peca inexistente");
    TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_valida, peca_id, 0) ==
                        RES_INVALIDO &&
                    item_ordem_usar_peca(db, ordem_valida, peca_id, -1) ==
                        RES_INVALIDO &&
                    item_ordem_usar_peca(db, ordem_valida, peca_id, 100001) ==
                        RES_INVALIDO,
                    "rejeitar quantidades invalidas");
    TESTE_VERIFICAR(contagens_iguais(antes, obter_contagens(db, peca_id)),
                    "falhas de validacao nao alteram registros");
}

static void verificar_item(sqlite3 *db, int ordem_id, int peca_id)
{
    ItemOrdem *lista = NULL;
    EventoHistorico *eventos = NULL;
    int quantidade = 0;
    int qtd_eventos = 0;
    Resultado resultado = item_ordem_listar(db, ordem_id, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 1 &&
                    lista[0].peca_id == peca_id &&
                    strcmp(lista[0].peca_codigo, "MEM16") == 0 &&
                    strcmp(lista[0].peca_nome, "Memória 16 GB") == 0 &&
                    lista[0].quantidade == 4,
                    "item usado lista codigo, nome e quantidade");
    free(lista);
    resultado = historico_listar_por_ordem(db, ordem_id, &eventos,
                                            &qtd_eventos);
    TESTE_VERIFICAR(resultado == RES_OK && qtd_eventos > 0 &&
                    strcmp(eventos[qtd_eventos - 1].tipo_evento, "PECA") == 0,
                    "uso de peca registra evento PECA");
    free(eventos);
}

static void testar_uso_inicial(sqlite3 *db, int ordem_id, int peca_id)
{
    Peca peca;
    MovimentoEstoque *movimentos = NULL;
    char motivo[64];
    int quantidade = 0;
    Contagens antes;

    TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_id, peca_id, 4) == RES_OK,
                    "usar quatro unidades em OS APROVADA");
    TESTE_VERIFICAR(peca_buscar_por_id(db, peca_id, &peca) == RES_OK &&
                    peca.quantidade == 6,
                    "baixa de estoque apos uso");
    snprintf(motivo, sizeof(motivo), "Uso na OS %d", ordem_id);
    TESTE_VERIFICAR(estoque_listar_movimentacoes(db, peca_id, &movimentos,
                                                  &quantidade) == RES_OK &&
                    quantidade == 2 &&
                    strcmp(movimentos[1].tipo, "SAIDA") == 0 &&
                    movimentos[1].ordem_id == ordem_id &&
                    strcmp(movimentos[1].motivo, motivo) == 0,
                    "movimentacao de saida vinculada a OS");
    free(movimentos);
    verificar_item(db, ordem_id, peca_id);
    antes = obter_contagens(db, peca_id);
    TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_id, peca_id, 7) ==
                        RES_ESTOQUE_INSUFICIENTE,
                    "estoque insuficiente rejeita uso");
    TESTE_VERIFICAR(contagens_iguais(antes, obter_contagens(db, peca_id)),
                    "estoque insuficiente nao altera dados");
}

static void testar_situacoes_permitidas(sqlite3 *db, int ordem_id,
                                        int peca_id)
{
    Peca peca;

    TESTE_VERIFICAR(ordem_atualizar_status(db, ordem_id, OS_AGUARDANDO_PECA,
                                           NULL) == RES_OK &&
                    item_ordem_usar_peca(db, ordem_id, peca_id, 1) == RES_OK,
                    "uso permitido em AGUARDANDO_PECA");
    TESTE_VERIFICAR(ordem_atualizar_status(db, ordem_id, OS_EM_REPARO, NULL)
                        == RES_OK &&
                    item_ordem_usar_peca(db, ordem_id, peca_id, 1) == RES_OK,
                    "uso permitido em EM_REPARO");
    TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_id, peca_id, 4) == RES_OK &&
                    peca_buscar_por_id(db, peca_id, &peca) == RES_OK &&
                    peca.quantidade == 0,
                    "usar restante do estoque");
    {
        Peca *baixas = NULL;
        int quantidade = 0;
        Resultado resultado = peca_listar_estoque_baixo(db, &baixas,
                                                         &quantidade);
        TESTE_VERIFICAR(resultado == RES_OK && quantidade == 1 &&
                        baixas[0].id == peca_id &&
                        baixas[0].quantidade == 0,
                        "peca zerada aparece no estoque baixo");
        free(baixas);
    }
}

static void testar_itens_vazios(sqlite3 *db, int ordem_id)
{
    ItemOrdem *lista = (ItemOrdem *)1;
    int quantidade = -1;

    TESTE_VERIFICAR(item_ordem_listar(db, ordem_id, &lista, &quantidade) ==
                        RES_OK &&
                    lista == NULL && quantidade == 0,
                    "OS sem pecas retorna lista vazia");
    TESTE_VERIFICAR(item_ordem_listar(db, 999999, &lista, &quantidade) ==
                        RES_NAO_ENCONTRADO,
                    "listar pecas de OS inexistente");
    free(lista);
}

static void testar_fechamento_e_cancelamento(sqlite3 *db, int equipamento_id,
                                            int ordem_id, int peca_id)
{
    int ordem_cancelada = 0;
    Contagens antes;

    TESTE_VERIFICAR(ordem_atualizar_status(db, ordem_id, OS_CONCLUIDA, NULL)
                        == RES_OK &&
                    item_ordem_usar_peca(db, ordem_id, peca_id, 1) ==
                        RES_REGRA_NEGOCIO,
                    "CONCLUIDA rejeita uso de peca");
    TESTE_VERIFICAR(ordem_atualizar_status(db, ordem_id, OS_ENTREGUE, NULL)
                        == RES_OK &&
                    item_ordem_usar_peca(db, ordem_id, peca_id, 1) ==
                        RES_REGRA_NEGOCIO,
                    "ENTREGUE rejeita uso de peca");
    TESTE_VERIFICAR(ordem_abrir(db, equipamento_id, 0, "Cancelar",
                                &ordem_cancelada) == RES_OK &&
                    ordem_atualizar_status(db, ordem_cancelada, OS_CANCELADA,
                                           "Cancelada pelo cliente") == RES_OK,
                    "preparar OS CANCELADA");
    antes = obter_contagens(db, peca_id);
    TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_cancelada, peca_id, 1) ==
                        RES_REGRA_NEGOCIO &&
                    contagens_iguais(antes, obter_contagens(db, peca_id)),
                    "CANCELADA rejeita uso sem alterar dados");
}

static void testar_rollback_trigger(sqlite3 *db, int ordem_id, int peca_id,
                                    const char *nome, const char *sql_criar,
                                    const char *sql_remover, int deve_sucesso)
{
    Contagens antes;
    int codigo = sqlite3_exec(db, sql_criar, NULL, NULL, NULL);

    TESTE_VERIFICAR(codigo == SQLITE_OK, nome);
    if (codigo != SQLITE_OK) return;
    antes = obter_contagens(db, peca_id);
    if (!deve_sucesso) {
        TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_id, peca_id, 2) !=
                            RES_OK,
                        "falha injetada rejeita uso da peca");
        TESTE_VERIFICAR(contagens_iguais(antes, obter_contagens(db, peca_id)),
                        "rollback restaura estoque e todas as contagens");
    }
    codigo = sqlite3_exec(db, sql_remover, NULL, NULL, NULL);
    TESTE_VERIFICAR(codigo == SQLITE_OK, "remover trigger de falha");
    if (deve_sucesso) {
        TESTE_VERIFICAR(item_ordem_usar_peca(db, ordem_id, peca_id, 2) ==
                            RES_OK,
                        "uso funciona apos remover trigger de historico");
    }
}

static void testar_listagem_ordenada(sqlite3 *db, int ordem_id)
{
    ItemOrdem *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = item_ordem_listar(db, ordem_id, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 5,
                    "listar todos os itens em ordem crescente");
    if (quantidade == 5) {
        for (i = 1; i < quantidade; i++) {
            TESTE_VERIFICAR(lista[i - 1].id < lista[i].id,
                            "itens em ordem cronologica");
        }
    }
    free(lista);
}

static void testar_invariante(sqlite3 *db)
{
    static const char sql[] =
        "SELECT COUNT(*) FROM pecas p WHERE p.quantidade != "
        "COALESCE((SELECT SUM(CASE WHEN m.tipo = 'ENTRADA' THEN m.quantidade "
        "ELSE -m.quantidade END) FROM movimentacoes_estoque m "
        "WHERE m.peca_id = p.id), 0);";
    sqlite3_stmt *consulta = NULL;
    int incorretas = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) == SQLITE_OK &&
        sqlite3_step(consulta) == SQLITE_ROW) {
        incorretas = sqlite3_column_int(consulta, 0);
    }
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) {
        incorretas = -1;
    }
    TESTE_VERIFICAR(incorretas == 0,
                    "estoque corresponde as entradas menos saidas");
}

static void testar_persistencia(sqlite3 **db, int ordem_id, int peca_id)
{
    Peca peca;
    ItemOrdem *lista = NULL;
    MovimentoEstoque *movimentos = NULL;
    int quantidade = 0;

    db_fechar(*db);
    *db = NULL;
    if (db_abrir(db, CAMINHO_BANCO) != RES_OK) {
        TESTE_VERIFICAR(0, "reabrir banco de itens");
        return;
    }
    TESTE_VERIFICAR(1, "reabrir banco de itens");
    TESTE_VERIFICAR(peca_buscar_por_id(*db, peca_id, &peca) == RES_OK &&
                    peca.quantidade == 8,
                    "estoque persiste apos reabrir");
    TESTE_VERIFICAR(item_ordem_listar(*db, ordem_id, &lista, &quantidade) ==
                        RES_OK &&
                    quantidade == 5,
                    "itens persistem apos reabrir");
    free(lista);
    quantidade = 0;
    TESTE_VERIFICAR(estoque_listar_movimentacoes(*db, peca_id, &movimentos,
                                                  &quantidade) == RES_OK &&
                    quantidade == 7,
                    "movimentacoes persistem apos reabrir");
    free(movimentos);
}

static void executar_testes(sqlite3 *db, DadosTeste *dados)
{
    static const char trigger_item[] =
        "CREATE TRIGGER falha_item BEFORE INSERT ON itens_ordem "
        "BEGIN SELECT RAISE(ABORT, 'falha forcada'); END;";
    static const char drop_item[] = "DROP TRIGGER falha_item;";
    static const char trigger_hist[] =
        "CREATE TRIGGER falha_hist BEFORE INSERT ON historico_ordens "
        "WHEN NEW.tipo_evento = 'PECA' "
        "BEGIN SELECT RAISE(ABORT, 'falha forcada'); END;";
    static const char drop_hist[] = "DROP TRIGGER falha_hist;";
    int preparados = criar_dados(db, dados);

    TESTE_VERIFICAR(preparados,
                    "preparar dados e OS APROVADA");
    if (!preparados) return;
    testar_itens_vazios(db, dados->ordem_aberta);
    testar_aberta_e_entradas_invalidas(db, dados->ordem_aberta,
                                       dados->ordem_aprovada, dados->peca_id);
    testar_uso_inicial(db, dados->ordem_aprovada, dados->peca_id);
    testar_situacoes_permitidas(db, dados->ordem_aprovada, dados->peca_id);
    TESTE_VERIFICAR(peca_registrar_entrada(db, dados->peca_id, 10, "Reposição")
                        ==
                        RES_OK,
                    "repor estoque para testes de rollback");
    testar_rollback_trigger(db, dados->ordem_aprovada, dados->peca_id,
                            "criar trigger de falha em item",
                            trigger_item, drop_item, 0);
    testar_rollback_trigger(db, dados->ordem_aprovada, dados->peca_id,
                            "criar trigger de falha em historico",
                            trigger_hist, drop_hist, 1);
    testar_fechamento_e_cancelamento(db, dados->equipamento_id,
                                     dados->ordem_aprovada, dados->peca_id);
    testar_listagem_ordenada(db, dados->ordem_aprovada);
    testar_invariante(db);
}

int main(void)
{
    sqlite3 *db = NULL;
    DadosTeste dados = {0};

    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        TESTE_VERIFICAR(0, "remover banco anterior");
        return teste_codigo_saida();
    }
    if (db_abrir(&db, CAMINHO_BANCO) != RES_OK) {
        TESTE_VERIFICAR(0, "abrir banco de itens");
        return teste_codigo_saida();
    }
    executar_testes(db, &dados);
    testar_persistencia(&db, dados.ordem_aprovada, dados.peca_id);
    testar_invariante(db);
    db_fechar(db);
    return teste_codigo_saida();
}
