#include "menu_relatorios.h"

#include "pecas.h"
#include "relatorios.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static const char *const relatorio_meses[12] = {
    "Jan", "Fev", "Mar", "Abr", "Mai", "Jun",
    "Jul", "Ago", "Set", "Out", "Nov", "Dez"
};
static const char *const relatorio_siglas[OS_STATUS_TOTAL] = {
    "ABRT", "DIAG", "AGAP", "APRV", "AGPC", "REPR", "CONC", "ENTR", "CANC"
};

static int relatorio_ler_ano(int *ano)
{
    char texto[32];
    char *fim;
    long valor;
    Resultado resultado = util_ler_linha(
        "Ano (Enter = ano atual): ", texto, sizeof(texto));

    if (resultado != RES_OK) return 0;
    if (texto[0] == '\0') {
        *ano = util_ano_atual();
        return *ano >= 2000 && *ano <= 2100;
    }
    valor = strtol(texto, &fim, 10);
    if (*fim != '\0' || valor < 2000 || valor > 2100) {
        util_erro(util_texto_resultado(RES_INVALIDO));
        return 0;
    }
    *ano = (int)valor;
    return 1;
}

static void relatorio_resumo_tela(sqlite3 *db)
{
    ResumoOrdens resumo;
    int status;
    Resultado resultado = relatorio_resumo_ordens(db, &resumo);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    printf("Total: %d\nEm aberto: %d\nConcluídas: %d\n",
           resumo.total, resumo.em_aberto, resumo.concluidas);
    for (status = 0; status < OS_STATUS_TOTAL; status++) {
        printf("%s: %d\n", os_status_rotulo((StatusOS)status),
               resumo.por_status[status]);
    }
}

static void relatorio_imprimir_cabecalho_mes(void)
{
    int status;

    util_imprimir_coluna("Mês", 4);
    for (status = 0; status < OS_STATUS_TOTAL; status++) {
        util_imprimir_coluna(relatorio_siglas[status], 4);
    }
    util_imprimir_coluna("Total", 5);
    putchar('\n');
}

static void relatorio_imprimir_linha_mes(
    int matriz[12][OS_STATUS_TOTAL], int mes)
{
    int status;

    util_imprimir_coluna(relatorio_meses[mes - 1], 4);
    for (status = 0; status < OS_STATUS_TOTAL; status++) {
        char texto[12];
        snprintf(texto, sizeof(texto), "%d", matriz[mes - 1][status]);
        util_imprimir_coluna(texto, 4);
    }
    {
        char texto[12];
        snprintf(texto, sizeof(texto), "%d",
                 relatorio_total_mes(matriz, mes));
        util_imprimir_coluna(texto, 5);
    }
    putchar('\n');
}

static void relatorio_imprimir_total_status(
    int matriz[12][OS_STATUS_TOTAL])
{
    int status;

    util_imprimir_coluna("Total", 4);
    for (status = 0; status < OS_STATUS_TOTAL; status++) {
        char texto[12];
        snprintf(texto, sizeof(texto), "%d",
                 relatorio_total_status(matriz, status));
        util_imprimir_coluna(texto, 4);
    }
    {
        int total = 0;
        int mes;
        char texto[12];
        for (mes = 1; mes <= 12; mes++) {
            total += relatorio_total_mes(matriz, mes);
        }
        snprintf(texto, sizeof(texto), "%d", total);
        util_imprimir_coluna(texto, 5);
    }
    putchar('\n');
}

static void relatorio_imprimir_legenda(void)
{
    int status;

    for (status = 0; status < OS_STATUS_TOTAL; status++) {
        printf("%s = %s", relatorio_siglas[status],
               os_status_rotulo((StatusOS)status));
        if (status % 2 == 0 && status + 1 < OS_STATUS_TOTAL) {
            printf(" | ");
        } else {
            putchar('\n');
        }
    }
}

static void relatorio_mensal_tela(sqlite3 *db)
{
    int matriz[12][OS_STATUS_TOTAL];
    int ano;
    int mes;
    Resultado resultado;

    if (!relatorio_ler_ano(&ano)) return;
    resultado = relatorio_ordens_por_mes(db, ano, matriz);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    printf("Ordens por mês - %d\n", ano);
    relatorio_imprimir_cabecalho_mes();
    for (mes = 1; mes <= 12; mes++) {
        relatorio_imprimir_linha_mes(matriz, mes);
    }
    relatorio_imprimir_total_status(matriz);
    relatorio_imprimir_legenda();
}

static void relatorio_imprimir_peca_baixa(const Peca *peca)
{
    char quantidade[16];
    char minimo[16];

    snprintf(quantidade, sizeof(quantidade), "%d", peca->quantidade);
    snprintf(minimo, sizeof(minimo), "%d", peca->estoque_minimo);
    util_imprimir_coluna(peca->codigo, 14);
    util_imprimir_coluna(peca->nome, 26);
    util_imprimir_coluna(quantidade, 10);
    util_imprimir_coluna(minimo, 10);
    putchar('\n');
}

static void relatorio_estoque_baixo_tela(sqlite3 *db)
{
    Peca *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = peca_listar_estoque_baixo(db, &lista, &quantidade);

    if (resultado != RES_OK) util_erro(util_texto_resultado(resultado));
    else if (quantidade == 0) puts("Nenhuma peça com estoque baixo.");
    else {
        util_imprimir_coluna("Código", 14);
        util_imprimir_coluna("Nome", 26);
        util_imprimir_coluna("Quantidade", 10);
        util_imprimir_coluna("Mínimo", 10);
        putchar('\n');
        for (i = 0; i < quantidade; i++) {
            relatorio_imprimir_peca_baixa(&lista[i]);
        }
    }
    free(lista);
}

static void relatorio_imprimir_movimento(
    const MovimentoEstoque *movimento)
{
    char data[20];
    char quantidade[16];
    char ordem[16];

    util_formatar_data(movimento->data_hora, data, sizeof(data));
    snprintf(quantidade, sizeof(quantidade), "%d", movimento->quantidade);
    if (movimento->ordem_id == 0) snprintf(ordem, sizeof(ordem), "-");
    else snprintf(ordem, sizeof(ordem), "%d", movimento->ordem_id);
    util_imprimir_coluna(data, 19);
    util_imprimir_coluna(movimento->peca_codigo, 12);
    util_imprimir_coluna(movimento->peca_nome, 22);
    util_imprimir_coluna(movimento->tipo, 8);
    util_imprimir_coluna(quantidade, 10);
    util_imprimir_coluna(ordem, 8);
    util_imprimir_coluna(movimento->motivo, 24);
    putchar('\n');
}

static void relatorio_movimentacoes_tela(sqlite3 *db)
{
    MovimentoEstoque *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = estoque_listar_movimentacoes(
        db, 0, &lista, &quantidade);

    if (resultado != RES_OK) util_erro(util_texto_resultado(resultado));
    else if (quantidade == 0) puts("Nenhuma movimentação registrada.");
    else {
        util_imprimir_coluna("Data", 19);
        util_imprimir_coluna("Código", 12);
        util_imprimir_coluna("Nome da peça", 22);
        util_imprimir_coluna("Tipo", 8);
        util_imprimir_coluna("Quantidade", 10);
        util_imprimir_coluna("Ordem", 8);
        util_imprimir_coluna("Motivo", 24);
        putchar('\n');
        for (i = 0; i < quantidade; i++) {
            relatorio_imprimir_movimento(&lista[i]);
        }
    }
    free(lista);
}

void menu_relatorios(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("RELATÓRIOS");
        puts("1 - Resumo de ordens\n2 - Ordens por mês");
        puts("3 - Peças com estoque baixo\n4 - Movimentações de estoque");
        puts("0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 4, &opcao);
        if (resultado != RES_OK) return;
        switch (opcao) {
            case 0: return;
            case 1: relatorio_resumo_tela(db); break;
            case 2: relatorio_mensal_tela(db); break;
            case 3: relatorio_estoque_baixo_tela(db); break;
            case 4: relatorio_movimentacoes_tela(db); break;
            default: break;
        }
    }
}
