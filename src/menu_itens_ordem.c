#include "menu_itens_ordem.h"

#include "itens_ordem.h"
#include "ordens.h"
#include "pecas.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int item_ler_id(const char *mensagem, int *id)
{
    Resultado resultado = util_ler_inteiro(mensagem, 1, INT_MAX, id);

    if (resultado == RES_OK) return 1;
    util_erro(util_texto_resultado(resultado));
    return 0;
}

static void item_imprimir_linha(const ItemOrdem *item)
{
    char quantidade[16];
    char data[20];

    snprintf(quantidade, sizeof(quantidade), "%d", item->quantidade);
    util_formatar_data(item->data_uso, data, sizeof(data));
    util_imprimir_coluna(item->peca_codigo, 16);
    util_imprimir_coluna(item->peca_nome, 28);
    util_imprimir_coluna(quantidade, 12);
    util_imprimir_coluna(data, 19);
    putchar('\n');
}

static void item_listar_tela(sqlite3 *db, int ordem_id)
{
    ItemOrdem *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = item_ordem_listar(db, ordem_id, &lista,
                                              &quantidade);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhuma peça usada nesta ordem.");
    } else {
        util_imprimir_coluna("Código", 16);
        util_imprimir_coluna("Nome", 28);
        util_imprimir_coluna("Quantidade", 12);
        util_imprimir_coluna("Data", 19);
        putchar('\n');
        for (i = 0; i < quantidade; i++) item_imprimir_linha(&lista[i]);
    }
    free(lista);
}

static int item_mostrar_ordem(sqlite3 *db, int *ordem_id,
                              OrdemServico *ordem)
{
    int status;
    Resultado resultado;

    if (!item_ler_id("ID da ordem: ", ordem_id)) return 0;
    resultado = ordem_buscar_por_id(db, *ordem_id, ordem);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    status = os_status_de_codigo(ordem->status);
    printf("Ordem %d - situação: %s\n", ordem->id,
           status >= 0 ? os_status_rotulo((StatusOS)status) : ordem->status);
    item_listar_tela(db, *ordem_id);
    return 1;
}

static Resultado item_ler_peca(sqlite3 *db, Peca *peca)
{
    char codigo[31];
    Resultado resultado = util_ler_texto_obrigatorio(
        "Código da peça: ", codigo, sizeof(codigo));

    if (resultado == RES_OK) {
        resultado = peca_buscar_por_codigo(db, codigo, peca);
    }
    if (resultado == RES_OK) {
        printf("%s - quantidade disponível: %d\n", peca->nome,
               peca->quantidade);
    }
    return resultado;
}

static Resultado item_ler_quantidade(int *quantidade, int *confirmado)
{
    Resultado resultado = util_ler_inteiro("Quantidade a usar: ", 1, 100000,
                                            quantidade);

    if (resultado == RES_OK) {
        resultado = util_ler_confirmacao("Confirmar uso? (S/N): ",
                                          confirmado);
    }
    return resultado;
}

static void item_exibir_estoque_atualizado(sqlite3 *db, int peca_id)
{
    Peca peca;
    Resultado resultado = peca_buscar_por_id(db, peca_id, &peca);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    printf("[OK] Peça registrada. Nova quantidade: %d.\n", peca.quantidade);
    if (peca.quantidade <= peca.estoque_minimo) {
        puts("Atenção: estoque baixo.");
    }
}

static void item_usar_tela(sqlite3 *db)
{
    OrdemServico ordem;
    Peca peca;
    int ordem_id;
    int quantidade;
    int confirmado = 0;
    Resultado resultado;

    if (!item_mostrar_ordem(db, &ordem_id, &ordem)) return;
    resultado = item_ler_peca(db, &peca);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    resultado = item_ler_quantidade(&quantidade, &confirmado);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    if (!confirmado) return;
    resultado = item_ordem_usar_peca(db, ordem_id, peca.id, quantidade);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    item_exibir_estoque_atualizado(db, peca.id);
}

static void item_listar_menu(sqlite3 *db)
{
    int ordem_id;

    if (item_ler_id("ID da ordem: ", &ordem_id)) {
        item_listar_tela(db, ordem_id);
    }
}

void menu_itens_ordem(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("PEÇAS DA ORDEM");
        puts("1 - Usar peça na ordem\n2 - Listar peças da ordem\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 2, &opcao);
        if (resultado != RES_OK) return;
        switch (opcao) {
            case 0: return;
            case 1: item_usar_tela(db); break;
            case 2: item_listar_menu(db); break;
            default: break;
        }
    }
}
