#include "clientes.h"
#include "equipamentos.h"
#include "ordens.h"
#include "relatorios.h"
#include "tecnicos.h"
#include "test_util.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_relatorios.sqlite"

static int criar_dependencias(sqlite3 *db, int *equipamento_id,
                              int *tecnico_id)
{
    Cliente cliente = {0};
    Equipamento equipamento = {0};
    Tecnico tecnico = {0};
    int cliente_id;

    snprintf(cliente.nome, sizeof(cliente.nome), "%s", "Cliente Relatorio");
    snprintf(cliente.documento, sizeof(cliente.documento), "%s",
             "12345678901");
    snprintf(cliente.telefone, sizeof(cliente.telefone), "%s", "11999999999");
    if (cliente_inserir(db, &cliente, &cliente_id) != RES_OK) return 0;
    equipamento.cliente_id = cliente_id;
    snprintf(equipamento.tipo, sizeof(equipamento.tipo), "%s", "NOTEBOOK");
    snprintf(equipamento.marca, sizeof(equipamento.marca), "%s", "Marca");
    snprintf(equipamento.modelo, sizeof(equipamento.modelo), "%s", "Modelo");
    snprintf(equipamento.numero_serie, sizeof(equipamento.numero_serie), "%s",
             "RELATORIO-1");
    if (equipamento_inserir(db, &equipamento, equipamento_id) != RES_OK) {
        return 0;
    }
    snprintf(tecnico.nome, sizeof(tecnico.nome), "%s", "Tecnico Relatorio");
    snprintf(tecnico.especialidade, sizeof(tecnico.especialidade), "%s",
             "Geral");
    return tecnico_inserir(db, &tecnico, tecnico_id) == RES_OK;
}

static int criar_ordem(sqlite3 *db, int equipamento_id, int *ordem_id)
{
    return ordem_abrir(db, equipamento_id, 0, "Teste de relatório",
                       ordem_id) == RES_OK;
}

static int ajustar_ordem(sqlite3 *db, int ordem_id, const char *status,
                         const char *data)
{
    sqlite3_stmt *consulta = NULL;
    static const char sql[] =
        "UPDATE ordens_servico SET status = ?1, data_abertura = ?2 "
        "WHERE id = ?3;";
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo == SQLITE_OK) {
        codigo = sqlite3_bind_text(consulta, 1, status, -1, SQLITE_TRANSIENT);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_bind_text(consulta, 2, data, -1, SQLITE_TRANSIENT);
    }
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 3, ordem_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) return 0;
    return codigo == SQLITE_DONE && sqlite3_changes(db) == 1;
}

static int criar_oito_ordens(sqlite3 *db, int equipamento_id)
{
    static const char *const status[8] = {
        "ABERTA", "ABERTA", "EM_DIAGNOSTICO", "CONCLUIDA",
        "ENTREGUE", "CANCELADA", "ABERTA", "EM_REPARO"
    };
    static const char *const datas[8] = {
        "2026-01-15 12:00:00", "2026-01-31 23:59:59",
        "2026-02-01 00:00:00", "2026-03-05 10:00:00",
        "2026-03-20 11:00:00", "2026-03-25 12:00:00",
        "2025-12-31 23:59:59", "2027-01-01 00:00:00"
    };
    int i;

    for (i = 0; i < 8; i++) {
        int ordem_id = 0;
        if (!criar_ordem(db, equipamento_id, &ordem_id) ||
            !ajustar_ordem(db, ordem_id, status[i], datas[i])) {
            return 0;
        }
    }
    return 1;
}

static int matriz_sem_ordens(int matriz[12][OS_STATUS_TOTAL]);

static void testar_vazio(sqlite3 *db)
{
    ResumoOrdens resumo;
    int matriz[12][OS_STATUS_TOTAL];
    Resultado resultado = relatorio_resumo_ordens(db, &resumo);

    TESTE_VERIFICAR(resultado == RES_OK && resumo.total == 0 &&
                    resumo.em_aberto == 0 && resumo.concluidas == 0,
                    "resumo sem ordens");
    TESTE_VERIFICAR(relatorio_ordens_por_mes(db, 2024, matriz) == RES_OK,
                    "matriz vazia sem ordens");
    TESTE_VERIFICAR(matriz_sem_ordens(matriz),
                    "matriz inteira zerada sem ordens");
}

static void testar_resumo(sqlite3 *db)
{
    ResumoOrdens resumo;
    int soma = 0;
    int i;
    Resultado resultado = relatorio_resumo_ordens(db, &resumo);

    TESTE_VERIFICAR(resultado == RES_OK && resumo.total == 8 &&
                    resumo.em_aberto == 5 && resumo.concluidas == 2,
                    "totais do resumo");
    TESTE_VERIFICAR(resumo.por_status[OS_ABERTA] == 3 &&
                    resumo.por_status[OS_EM_DIAGNOSTICO] == 1 &&
                    resumo.por_status[OS_CONCLUIDA] == 1 &&
                    resumo.por_status[OS_ENTREGUE] == 1 &&
                    resumo.por_status[OS_CANCELADA] == 1 &&
                    resumo.por_status[OS_EM_REPARO] == 1,
                    "contagem por situação");
    for (i = 0; i < OS_STATUS_TOTAL; i++) soma += resumo.por_status[i];
    TESTE_VERIFICAR(soma == resumo.total,
                    "soma por situação equivale ao total");
}

static int matriz_2026_correta(int matriz[12][OS_STATUS_TOTAL])
{
    int mes;
    int status;

    for (mes = 0; mes < 12; mes++) {
        for (status = 0; status < OS_STATUS_TOTAL; status++) {
            int esperado = 0;
            if (mes == 0 && status == OS_ABERTA) esperado = 2;
            if (mes == 1 && status == OS_EM_DIAGNOSTICO) esperado = 1;
            if (mes == 2 && (status == OS_CONCLUIDA ||
                             status == OS_ENTREGUE ||
                             status == OS_CANCELADA)) esperado = 1;
            if (matriz[mes][status] != esperado) return 0;
        }
    }
    return 1;
}

static int matriz_sem_ordens(int matriz[12][OS_STATUS_TOTAL])
{
    int mes;
    int status;

    for (mes = 0; mes < 12; mes++) {
        for (status = 0; status < OS_STATUS_TOTAL; status++) {
            if (matriz[mes][status] != 0) return 0;
        }
    }
    return 1;
}

static void testar_matriz_2026(sqlite3 *db)
{
    int matriz[12][OS_STATUS_TOTAL];
    int total = 0;
    int mes;
    int status;
    Resultado resultado = relatorio_ordens_por_mes(db, 2026, matriz);

    TESTE_VERIFICAR(resultado == RES_OK &&
                    matriz[0][OS_ABERTA] == 2 &&
                    matriz[1][OS_EM_DIAGNOSTICO] == 1 &&
                    matriz[2][OS_CONCLUIDA] == 1 &&
                    matriz[2][OS_ENTREGUE] == 1 &&
                    matriz[2][OS_CANCELADA] == 1,
                    "contagens mensais e situação corretas em 2026");
    TESTE_VERIFICAR(relatorio_total_mes(matriz, 1) == 2 &&
                    relatorio_total_mes(matriz, 2) == 1 &&
                    relatorio_total_mes(matriz, 3) == 3 &&
                    relatorio_total_status(matriz, OS_ABERTA) == 2,
                    "totais mensais e por situação");
    TESTE_VERIFICAR(matriz_2026_correta(matriz),
                    "demais células de 2026 são zero");
    for (mes = 0; mes < 12; mes++) {
        for (status = 0; status < OS_STATUS_TOTAL; status++) {
            total += matriz[mes][status];
        }
    }
    TESTE_VERIFICAR(total == 6, "total de ordens em 2026");
    TESTE_VERIFICAR(relatorio_total_mes(matriz, 0) == 0 &&
                    relatorio_total_mes(matriz, 13) == 0,
                    "rejeitar mês fora do intervalo");
}

static void testar_outros_anos(sqlite3 *db)
{
    int matriz[12][OS_STATUS_TOTAL];

    TESTE_VERIFICAR(relatorio_ordens_por_mes(db, 2025, matriz) == RES_OK &&
                    matriz[11][OS_ABERTA] == 1 &&
                    relatorio_total_mes(matriz, 12) == 1,
                    "ordem de dezembro de 2025");
    TESTE_VERIFICAR(relatorio_ordens_por_mes(db, 2027, matriz) == RES_OK &&
                    matriz[0][OS_EM_REPARO] == 1 &&
                    relatorio_total_mes(matriz, 1) == 1,
                    "ordem de janeiro de 2027");
    TESTE_VERIFICAR(relatorio_ordens_por_mes(db, 2024, matriz) == RES_OK,
                    "ano sem ordens retorna sucesso");
    TESTE_VERIFICAR(matriz_sem_ordens(matriz),
                    "ano sem ordens tem células zeradas");
}

static void testar_zeramento_e_limites(sqlite3 *db)
{
    int matriz[12][OS_STATUS_TOTAL];
    int mes;
    int status;
    Resultado resultado;

    for (mes = 0; mes < 12; mes++) {
        for (status = 0; status < OS_STATUS_TOTAL; status++) {
            matriz[mes][status] = 99;
        }
    }
    resultado = relatorio_ordens_por_mes(db, 2026, matriz);
    TESTE_VERIFICAR(resultado == RES_OK && matriz[0][OS_ABERTA] == 2 &&
                    matriz[1][OS_EM_DIAGNOSTICO] == 1 &&
                    matriz[2][OS_CONCLUIDA] == 1,
                    "chamada substitui matriz previamente preenchida");
    TESTE_VERIFICAR(relatorio_ordens_por_mes(db, 1999, matriz) == RES_INVALIDO &&
                    relatorio_ordens_por_mes(db, 2101, matriz) == RES_INVALIDO,
                    "rejeitar ano fora do intervalo");
    TESTE_VERIFICAR(util_ano_atual() >= 2024 && util_ano_atual() <= 2100,
                    "obter ano atual");
}

int main(void)
{
    sqlite3 *db = NULL;
    int equipamento_id = 0;
    int tecnico_id = 0;

    (void)tecnico_id;
    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        TESTE_VERIFICAR(0, "remover banco anterior");
        return teste_codigo_saida();
    }
    if (db_abrir(&db, CAMINHO_BANCO) != RES_OK) {
        TESTE_VERIFICAR(0, "abrir banco de relatórios");
        return teste_codigo_saida();
    }
    TESTE_VERIFICAR(criar_dependencias(db, &equipamento_id, &tecnico_id),
                    "criar cliente, equipamento e técnico");
    testar_vazio(db);
    TESTE_VERIFICAR(criar_oito_ordens(db, equipamento_id),
                    "criar oito ordens com datas controladas");
    testar_resumo(db);
    testar_matriz_2026(db);
    testar_outros_anos(db);
    testar_zeramento_e_limites(db);
    db_fechar(db);
    return teste_codigo_saida();
}
