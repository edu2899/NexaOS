#include "db.h"
#include "ordens.h"
#include "pecas.h"
#include "relatorios.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define CAMINHO_BANCO "build\\test_dados_exemplo.sqlite"

static int consultar_inteiro(sqlite3 *db, const char *sql, int *valor)
{
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (codigo == SQLITE_ROW) {
        *valor = sqlite3_column_int(consulta, 0);
        codigo = SQLITE_OK;
    }
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) {
        codigo = SQLITE_ERROR;
    }
    return codigo == SQLITE_OK;
}

static void testar_contagens(sqlite3 *db)
{
    static const char *const tabelas[] = {
        "SELECT COUNT(*) FROM clientes;",
        "SELECT COUNT(*) FROM equipamentos;",
        "SELECT COUNT(*) FROM tecnicos;",
        "SELECT COUNT(*) FROM ordens_servico;",
        "SELECT COUNT(*) FROM pecas;",
        "SELECT COUNT(*) FROM itens_ordem;",
        "SELECT COUNT(*) FROM historico_ordens;",
        "SELECT COUNT(*) FROM movimentacoes_estoque;"
    };
    static const int esperados[] = {4, 5, 3, 8, 6, 5, 46, 11};
    int i;

    for (i = 0; i < (int)(sizeof(esperados) / sizeof(esperados[0])); i++) {
        int quantidade = -1;
        int consultou = consultar_inteiro(db, tabelas[i], &quantidade);
        TESTE_VERIFICAR(consultou && quantidade == esperados[i],
                        "contagem de linhas da tabela");
    }
}

static void testar_integridade(sqlite3 *db)
{
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, "PRAGMA foreign_key_check;",
                                    -1, &consulta, NULL);

    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) {
        codigo = SQLITE_ERROR;
    }
    TESTE_VERIFICAR(codigo == SQLITE_DONE,
                    "nenhuma violacao de chave estrangeira");
    TESTE_VERIFICAR(consultar_inteiro(
        db, "SELECT COUNT(*) FROM ordens_servico o WHERE NOT EXISTS "
            "(SELECT 1 FROM historico_ordens h WHERE h.ordem_id = o.id "
            "AND h.tipo_evento = 'ABERTURA');",
        &codigo) && codigo == 0, "todas as ordens tem evento de abertura");
    TESTE_VERIFICAR(consultar_inteiro(
        db, "SELECT COUNT(*) FROM (SELECT p.id FROM pecas p "
            "LEFT JOIN movimentacoes_estoque m ON m.peca_id = p.id "
            "GROUP BY p.id HAVING p.quantidade <> "
            "COALESCE(SUM(CASE WHEN m.tipo = 'ENTRADA' THEN m.quantidade "
            "WHEN m.tipo = 'SAIDA' THEN -m.quantidade ELSE 0 END), 0));",
        &codigo) && codigo == 0, "saldos conferem com as movimentacoes");
}

static void testar_ordens(sqlite3 *db)
{
    OrdemServico *lista = NULL;
    int quantidade = 0;
    Resultado resultado = ordem_listar(db, NULL, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 8,
                    "ordem_listar devolve oito ordens");
    free(lista);
}

static void testar_relatorios(sqlite3 *db)
{
    ResumoOrdens resumo = {0};
    int matriz[12][OS_STATUS_TOTAL] = {{0}};
    int soma_status = 0;
    int soma_meses = 0;
    int mes;
    int status;
    Resultado resultado = relatorio_resumo_ordens(db, &resumo);

    for (status = 0; status < OS_STATUS_TOTAL; status++) {
        soma_status += resumo.por_status[status];
    }
    TESTE_VERIFICAR(resultado == RES_OK && resumo.total == 8 &&
                    resumo.em_aberto == 5 && resumo.concluidas == 2 &&
                    resumo.por_status[OS_ABERTA] == 1 &&
                    resumo.por_status[OS_EM_DIAGNOSTICO] == 1 &&
                    resumo.por_status[OS_AGUARDANDO_APROVACAO] == 1 &&
                    resumo.por_status[OS_APROVADA] == 1 &&
                    resumo.por_status[OS_EM_REPARO] == 1 &&
                    resumo.por_status[OS_CONCLUIDA] == 1 &&
                    resumo.por_status[OS_ENTREGUE] == 1 &&
                    resumo.por_status[OS_CANCELADA] == 1 &&
                    soma_status == resumo.total,
                    "resumo e contagens por situacao corretos");
    resultado = relatorio_ordens_por_mes(db, 2026, matriz);
    for (mes = 0; mes < 12; mes++) {
        for (status = 0; status < OS_STATUS_TOTAL; status++) {
            soma_meses += matriz[mes][status];
        }
    }
    TESTE_VERIFICAR(resultado == RES_OK && soma_meses == resumo.total,
                    "matriz mensal soma oito ordens");
}

static void testar_estoque_baixo(sqlite3 *db)
{
    Peca *lista = NULL;
    int quantidade = 0;
    Resultado resultado = peca_listar_estoque_baixo(
        db, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 1 &&
                    lista != NULL && lista[0].id == 5 &&
                    lista[0].quantidade == 4 && lista[0].estoque_minimo == 5,
                    "somente a peca de estoque baixo e listada");
    free(lista);
}

int main(void)
{
    sqlite3 *db = NULL;
    Resultado resultado;

    vazamento_iniciar();
    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        TESTE_VERIFICAR(0, "remover banco anterior");
        return teste_codigo_saida();
    }
    resultado = db_abrir(&db, CAMINHO_BANCO);
    TESTE_VERIFICAR(resultado == RES_OK, "abrir banco de exemplo");
    if (resultado == RES_OK) {
        resultado = db_executar_script(db, "database/dados_exemplo.sql");
    }
    TESTE_VERIFICAR(resultado == RES_OK, "carregar dados de exemplo");
    if (resultado == RES_OK) {
        testar_contagens(db);
        testar_integridade(db);
        testar_ordens(db);
        testar_relatorios(db);
        testar_estoque_baixo(db);
    }
    db_fechar(db);
    return teste_codigo_saida();
}
