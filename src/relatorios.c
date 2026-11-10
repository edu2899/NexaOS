#include "relatorios.h"

#include "util.h"

#include <stdio.h>
#include <string.h>

static void relatorio_zerar_resumo(ResumoOrdens *resumo)
{
    int i;

    resumo->total = 0;
    resumo->em_aberto = 0;
    resumo->concluidas = 0;
    for (i = 0; i < OS_STATUS_TOTAL; i++) resumo->por_status[i] = 0;
}

static Resultado relatorio_ler_contagens(sqlite3 *db,
                                         ResumoOrdens *resumo)
{
    static const char sql[] =
        "SELECT status, COUNT(*) FROM ordens_servico GROUP BY status;";
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_prepare_v2(db, sql, -1, &consulta, NULL);

    if (codigo != SQLITE_OK) return db_traduzir_erro(db);
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        const unsigned char *texto = sqlite3_column_text(consulta, 0);
        int status = os_status_de_codigo(
            texto != NULL ? (const char *)texto : NULL);
        if (status >= 0) {
            resumo->por_status[status] = sqlite3_column_int(consulta, 1);
        }
    }
    if (codigo != SQLITE_DONE) {
        return db_finalizar(db, consulta, db_traduzir_erro(db));
    }
    return db_finalizar(db, consulta, RES_OK);
}

static void relatorio_calcular_resumo(ResumoOrdens *resumo)
{
    int i;

    for (i = 0; i < OS_STATUS_TOTAL; i++) {
        int quantidade = resumo->por_status[i];
        resumo->total += quantidade;
        if (i == OS_CONCLUIDA || i == OS_ENTREGUE) {
            resumo->concluidas += quantidade;
        }
        if (i != OS_CONCLUIDA && i != OS_ENTREGUE && i != OS_CANCELADA) {
            resumo->em_aberto += quantidade;
        }
    }
}

Resultado relatorio_resumo_ordens(sqlite3 *db, ResumoOrdens *saida)
{
    Resultado resultado;

    if (db == NULL || saida == NULL) return RES_INVALIDO;
    relatorio_zerar_resumo(saida);
    resultado = relatorio_ler_contagens(db, saida);
    if (resultado == RES_OK) relatorio_calcular_resumo(saida);
    return resultado;
}

static Resultado relatorio_preparar_ano(sqlite3 *db, int ano,
                                        sqlite3_stmt **consulta)
{
    static const char sql[] =
        "SELECT CAST(strftime('%m', data_abertura) AS INTEGER), status, "
        "COUNT(*) FROM ordens_servico "
        "WHERE strftime('%Y', data_abertura) = ?1 GROUP BY 1, 2;";
    char texto_ano[5];
    int codigo;

    snprintf(texto_ano, sizeof(texto_ano), "%04d", ano);
    codigo = sqlite3_prepare_v2(db, sql, -1, consulta, NULL);
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(*consulta, 1, texto_ano, 0);
    }
    return codigo == SQLITE_OK ? RES_OK : db_traduzir_erro(db);
}

static void relatorio_zerar_matriz(int matriz[12][OS_STATUS_TOTAL])
{
    int mes;
    int status;

    for (mes = 0; mes < 12; mes++) {
        for (status = 0; status < OS_STATUS_TOTAL; status++) {
            matriz[mes][status] = 0;
        }
    }
}

static Resultado relatorio_preencher_matriz(sqlite3 *db,
                                            sqlite3_stmt *consulta,
                                            int matriz[12][OS_STATUS_TOTAL])
{
    int codigo;

    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        const unsigned char *texto = sqlite3_column_text(consulta, 1);
        int mes = sqlite3_column_int(consulta, 0);
        int status = os_status_de_codigo(
            texto != NULL ? (const char *)texto : NULL);
        if (mes >= 1 && mes <= 12 && status >= 0) {
            matriz[mes - 1][status] = sqlite3_column_int(consulta, 2);
        }
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

Resultado relatorio_ordens_por_mes(sqlite3 *db, int ano,
                                   int matriz[12][OS_STATUS_TOTAL])
{
    sqlite3_stmt *consulta = NULL;
    Resultado resultado;

    if (db == NULL || matriz == NULL || ano < 2000 || ano > 2100) {
        return RES_INVALIDO;
    }
    relatorio_zerar_matriz(matriz);
    resultado = relatorio_preparar_ano(db, ano, &consulta);
    if (resultado == RES_OK) {
        resultado = relatorio_preencher_matriz(db, consulta, matriz);
    }
    if (consulta != NULL) resultado = db_finalizar(db, consulta, resultado);
    return resultado;
}

int relatorio_total_mes(int matriz[12][OS_STATUS_TOTAL], int mes)
{
    int total = 0;
    int status;

    if (matriz == NULL || mes < 1 || mes > 12) return 0;
    for (status = 0; status < OS_STATUS_TOTAL; status++) {
        total += matriz[mes - 1][status];
    }
    return total;
}

int relatorio_total_status(int matriz[12][OS_STATUS_TOTAL], int status)
{
    int total = 0;
    int mes;

    if (matriz == NULL || status < 0 || status >= OS_STATUS_TOTAL) return 0;
    for (mes = 0; mes < 12; mes++) total += matriz[mes][status];
    return total;
}
