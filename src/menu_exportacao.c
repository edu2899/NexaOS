/*
 * Solicita relatorios e exporta arquivos CSV para a pasta exports.
 * Os dados sao preparados pelo modulo de exportacao.
 */
#include "menu_exportacao.h"

#include "exportacao.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#endif

static Resultado exportacao_criar_pasta(void)
{
#ifdef _WIN32
    if (_mkdir("exports") == 0 || errno == EEXIST) return RES_OK;
    return RES_ERRO_ARQUIVO;
#else
    return RES_ERRO_ARQUIVO;
#endif
}

static int exportacao_montar_caminho(const char *nome, char *caminho,
                                     size_t tamanho)
{
    time_t agora = time(NULL);
    struct tm *data = localtime(&agora);
    char horario[16];
    int tamanho_caminho;

    if (data == NULL || strftime(horario, sizeof(horario), "%Y%m%d_%H%M%S",
                                 data) == 0) return 0;
    tamanho_caminho = snprintf(caminho, tamanho, "exports/relatorio_%s_%s.csv",
                               nome, horario);
    return tamanho_caminho >= 0 && (size_t)tamanho_caminho < tamanho;
}

static int exportacao_ler_ano(int *ano)
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

static Resultado exportacao_executar(sqlite3 *db, int opcao, int ano,
                                     const char *caminho)
{
    switch (opcao) {
        case 1: return exportar_resumo_ordens(db, caminho);
        case 2: return exportar_ordens_por_mes(db, ano, caminho);
        case 3: return exportar_estoque_baixo(db, caminho);
        case 4: return exportar_movimentacoes(db, caminho);
        default: return RES_INVALIDO;
    }
}

static void exportacao_gerar(sqlite3 *db, int opcao)
{
    char nome[48];
    char caminho[160];
    int ano = 0;
    Resultado resultado;

    if (opcao == 2 && !exportacao_ler_ano(&ano)) return;
    if (opcao == 2) snprintf(nome, sizeof(nome), "ordens_por_mes_%d", ano);
    else if (opcao == 1) snprintf(nome, sizeof(nome), "resumo_ordens");
    else if (opcao == 3) snprintf(nome, sizeof(nome), "estoque_baixo");
    else snprintf(nome, sizeof(nome), "movimentacoes");
    resultado = exportacao_criar_pasta();
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    if (!exportacao_montar_caminho(nome, caminho, sizeof(caminho))) {
        util_erro(util_texto_resultado(RES_ERRO_ARQUIVO));
        return;
    }
    resultado = exportacao_executar(db, opcao, ano, caminho);
    if (resultado == RES_OK) printf("Arquivo gerado: %s\n", caminho);
    else util_erro(util_texto_resultado(resultado));
}

void menu_exportacao(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("EXPORTAR DADOS");
        puts("1 - Resumo de ordens\n2 - Ordens por mês");
        puts("3 - Estoque baixo\n4 - Movimentações\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 4, &opcao);
        if (resultado != RES_OK) return;
        if (opcao == 0) return;
        exportacao_gerar(db, opcao);
    }
}
