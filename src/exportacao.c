/*
 * Gera arquivos CSV com relatorios e dados de estoque.
 * Trata campos especiais e neutraliza valores interpretaveis como formulas.
 */
#include "exportacao.h"

#include "pecas.h"
#include "relatorios.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int csv_precisa_aspas(const char *texto)
{
    return strpbrk(texto, ";\"\r\n") != NULL;
}

static int csv_tem_prefixo_formula(const char *texto)
{
    const char *inicio = texto;

    if (texto[0] == '\t' || texto[0] == '\r') return 1;
    while (*inicio == ' ' || *inicio == '\t' || *inicio == '\r' ||
           *inicio == '\n') {
        inicio++;
    }
    return *inicio == '=' || *inicio == '+' || *inicio == '-' ||
           *inicio == '@';
}

static int csv_escrever_texto(FILE *arquivo, const char *texto)
{
    const char *cursor = texto != NULL ? texto : "";
    int aspas = csv_precisa_aspas(cursor);

    if (aspas && fprintf(arquivo, "\"") < 0) return 0;
    if (csv_tem_prefixo_formula(cursor) && fprintf(arquivo, "'") < 0) {
        return 0;
    }
    while (*cursor != '\0') {
        if (*cursor == '"' && fprintf(arquivo, "\"") < 0) return 0;
        if (fprintf(arquivo, "%c", *cursor) < 0) return 0;
        cursor++;
    }
    return !aspas || fprintf(arquivo, "\"") >= 0;
}

static int csv_escrever_numero(FILE *arquivo, int valor)
{
    return fprintf(arquivo, "%d", valor) >= 0;
}

static int csv_separador(FILE *arquivo)
{
    return fprintf(arquivo, ";") >= 0;
}

static int csv_fim_linha(FILE *arquivo)
{
    return fprintf(arquivo, "\r\n") >= 0;
}

static FILE *csv_abrir(const char *caminho)
{
    FILE *arquivo = fopen(caminho, "wb");

    if (arquivo == NULL) return NULL;
    if (fprintf(arquivo, "%s", "\xEF\xBB\xBF") < 0) {
        if (fclose(arquivo) != 0) {
            (void)remove(caminho);
            return NULL;
        }
        (void)remove(caminho);
        return NULL;
    }
    return arquivo;
}

static Resultado csv_fechar(FILE *arquivo, const char *caminho, int sucesso)
{
    int codigo = fclose(arquivo);

    if (!sucesso || codigo != 0) {
        (void)remove(caminho);
        return RES_ERRO_ARQUIVO;
    }
    return RES_OK;
}

static int csv_campo(FILE *arquivo, const char *texto, int primeiro)
{
    return (primeiro || csv_separador(arquivo)) &&
           csv_escrever_texto(arquivo, texto);
}

static int csv_campo_numero(FILE *arquivo, int valor, int primeiro)
{
    return (primeiro || csv_separador(arquivo)) &&
           csv_escrever_numero(arquivo, valor);
}

static int csv_escrever_resumo(FILE *arquivo, const ResumoOrdens *resumo)
{
    int status;
    int ok = csv_campo(arquivo, "Situacao", 1) &&
             csv_campo(arquivo, "Quantidade", 0) && csv_fim_linha(arquivo);

    for (status = 0; ok && status < OS_STATUS_TOTAL; status++) {
        ok = csv_campo(arquivo, os_status_rotulo((StatusOS)status), 1) &&
             csv_campo_numero(arquivo, resumo->por_status[status], 0) &&
             csv_fim_linha(arquivo);
    }
    ok = ok && csv_campo(arquivo, "Total", 1) &&
         csv_campo_numero(arquivo, resumo->total, 0) && csv_fim_linha(arquivo);
    ok = ok && csv_campo(arquivo, "Em aberto", 1) &&
         csv_campo_numero(arquivo, resumo->em_aberto, 0) && csv_fim_linha(arquivo);
    return ok && csv_campo(arquivo, "Concluidas", 1) &&
           csv_campo_numero(arquivo, resumo->concluidas, 0) &&
           csv_fim_linha(arquivo);
}

Resultado exportar_resumo_ordens(sqlite3 *db, const char *caminho)
{
    ResumoOrdens resumo;
    FILE *arquivo;
    Resultado resultado;
    int sucesso;

    if (db == NULL || caminho == NULL || caminho[0] == '\0') return RES_INVALIDO;
    resultado = relatorio_resumo_ordens(db, &resumo);
    if (resultado != RES_OK) return resultado;
    arquivo = csv_abrir(caminho);
    if (arquivo == NULL) return RES_ERRO_ARQUIVO;
    sucesso = csv_escrever_resumo(arquivo, &resumo);
    return csv_fechar(arquivo, caminho, sucesso);
}

static int csv_escrever_cabecalho_meses(FILE *arquivo)
{
    int status;
    int ok = csv_campo(arquivo, "Mes", 1);

    for (status = 0; ok && status < OS_STATUS_TOTAL; status++) {
        ok = csv_campo(arquivo, os_status_rotulo((StatusOS)status), 0);
    }
    return ok && csv_campo(arquivo, "Total", 0) && csv_fim_linha(arquivo);
}

static int csv_escrever_linha_mes(FILE *arquivo,
                                 int matriz[12][OS_STATUS_TOTAL], int mes,
                                 const char *rotulo)
{
    int status;
    int ok = csv_campo(arquivo, rotulo, 1);

    for (status = 0; ok && status < OS_STATUS_TOTAL; status++) {
        ok = csv_campo_numero(arquivo, matriz[mes][status], 0);
    }
    return ok && csv_campo_numero(arquivo, relatorio_total_mes(matriz, mes + 1),
                                  0) &&
           csv_fim_linha(arquivo);
}

static int csv_escrever_total_mes(FILE *arquivo,
                                 int matriz[12][OS_STATUS_TOTAL])
{
    int status;
    int ok = csv_campo(arquivo, "Total", 1);

    for (status = 0; ok && status < OS_STATUS_TOTAL; status++) {
        ok = csv_campo_numero(arquivo,
                              relatorio_total_status(matriz, status), 0);
    }
    {
        int total = 0;
        int mes;
        for (mes = 1; mes <= 12; mes++) {
            total += relatorio_total_mes(matriz, mes);
        }
        return ok && csv_campo_numero(arquivo, total, 0) &&
               csv_fim_linha(arquivo);
    }
}

Resultado exportar_ordens_por_mes(sqlite3 *db, int ano,
                                  const char *caminho)
{
    static const char *const meses[12] = {
        "Jan", "Fev", "Mar", "Abr", "Mai", "Jun",
        "Jul", "Ago", "Set", "Out", "Nov", "Dez"
    };
    int matriz[12][OS_STATUS_TOTAL];
    FILE *arquivo;
    Resultado resultado;
    int mes;
    int sucesso;

    if (db == NULL || caminho == NULL || caminho[0] == '\0' ||
        ano < 2000 || ano > 2100) return RES_INVALIDO;
    resultado = relatorio_ordens_por_mes(db, ano, matriz);
    if (resultado != RES_OK) return resultado;
    arquivo = csv_abrir(caminho);
    if (arquivo == NULL) return RES_ERRO_ARQUIVO;
    sucesso = csv_escrever_cabecalho_meses(arquivo);
    for (mes = 0; sucesso && mes < 12; mes++) {
        sucesso = csv_escrever_linha_mes(arquivo, matriz, mes, meses[mes]);
    }
    if (sucesso) sucesso = csv_escrever_total_mes(arquivo, matriz);
    return csv_fechar(arquivo, caminho, sucesso);
}

static int csv_escrever_linha_peca(FILE *arquivo, const Peca *peca)
{
    return csv_campo(arquivo, peca->codigo, 1) &&
        csv_campo(arquivo, peca->nome, 0) &&
        csv_campo_numero(arquivo, peca->quantidade, 0) &&
        csv_campo_numero(arquivo, peca->estoque_minimo, 0) &&
        csv_fim_linha(arquivo);
}

static int csv_escrever_pecas(FILE *arquivo, const Peca *lista, int total)
{
    int i;
    int ok = csv_campo(arquivo, "Codigo", 1) &&
        csv_campo(arquivo, "Nome", 0) &&
        csv_campo(arquivo, "Quantidade", 0) &&
        csv_campo(arquivo, "Minimo", 0) && csv_fim_linha(arquivo);

    for (i = 0; ok && i < total; i++) {
        ok = csv_escrever_linha_peca(arquivo, &lista[i]);
    }
    return ok;
}

Resultado exportar_estoque_baixo(sqlite3 *db, const char *caminho)
{
    Peca *lista = NULL;
    int quantidade = 0;
    FILE *arquivo;
    Resultado resultado;
    int sucesso;

    if (db == NULL || caminho == NULL || caminho[0] == '\0') return RES_INVALIDO;
    resultado = peca_listar_estoque_baixo(db, &lista, &quantidade);
    if (resultado != RES_OK) return resultado;
    arquivo = csv_abrir(caminho);
    if (arquivo == NULL) {
        free(lista);
        return RES_ERRO_ARQUIVO;
    }
    sucesso = csv_escrever_pecas(arquivo, lista, quantidade);
    free(lista);
    return csv_fechar(arquivo, caminho, sucesso);
}

static int csv_escrever_movimento(FILE *arquivo,
                                  const MovimentoEstoque *movimento)
{
    char data[20];

    util_formatar_data(movimento->data_hora, data, sizeof(data));
    return csv_campo(arquivo, data, 1) &&
        csv_campo(arquivo, movimento->peca_codigo, 0) &&
        csv_campo(arquivo, movimento->peca_nome, 0) &&
        csv_campo(arquivo, movimento->tipo, 0) &&
        csv_campo_numero(arquivo, movimento->quantidade, 0) &&
        (movimento->ordem_id == 0
             ? csv_campo(arquivo, "", 0)
             : csv_campo_numero(arquivo, movimento->ordem_id, 0)) &&
        csv_campo(arquivo, movimento->motivo, 0) &&
        csv_fim_linha(arquivo);
}

static int csv_escrever_movimentacoes(FILE *arquivo,
                                      const MovimentoEstoque *lista,
                                      int quantidade)
{
    int i;
    int ok = csv_campo(arquivo, "Data", 1) &&
        csv_campo(arquivo, "Codigo", 0) &&
        csv_campo(arquivo, "Peca", 0) &&
        csv_campo(arquivo, "Tipo", 0) &&
        csv_campo(arquivo, "Quantidade", 0) &&
        csv_campo(arquivo, "Ordem", 0) &&
        csv_campo(arquivo, "Motivo", 0) && csv_fim_linha(arquivo);

    for (i = 0; ok && i < quantidade; i++) {
        ok = csv_escrever_movimento(arquivo, &lista[i]);
    }
    return ok;
}

Resultado exportar_movimentacoes(sqlite3 *db, const char *caminho)
{
    MovimentoEstoque *lista = NULL;
    int quantidade = 0;
    FILE *arquivo;
    Resultado resultado;
    int sucesso;

    if (db == NULL || caminho == NULL || caminho[0] == '\0') return RES_INVALIDO;
    resultado = estoque_listar_movimentacoes(db, 0, &lista, &quantidade);
    if (resultado != RES_OK) return resultado;
    arquivo = csv_abrir(caminho);
    if (arquivo == NULL) {
        free(lista);
        return RES_ERRO_ARQUIVO;
    }
    sucesso = csv_escrever_movimentacoes(arquivo, lista, quantidade);
    free(lista);
    return csv_fechar(arquivo, caminho, sucesso);
}
