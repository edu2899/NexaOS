/*
 * Apresenta as operacoes disponiveis para o controle de estoque.
 * As consultas e alteracoes sao realizadas pelo modulo de pecas.
 */
#include "menu_estoque.h"

#include "pecas.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

static int estoque_ler_codigo(char *codigo, size_t tamanho)
{
    Resultado resultado = util_ler_texto_obrigatorio(
        "Código da peça: ", codigo, tamanho);

    if (resultado == RES_OK) return 1;
    util_erro(util_texto_resultado(resultado));
    return 0;
}

static int estoque_buscar_tela(sqlite3 *db, Peca *peca)
{
    char codigo[31];
    Resultado resultado;

    if (!estoque_ler_codigo(codigo, sizeof(codigo))) return 0;
    resultado = peca_buscar_por_codigo(db, codigo, peca);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    return 1;
}

static void estoque_cadastrar_tela(sqlite3 *db)
{
    Peca peca = {0};
    int id_novo;
    Resultado resultado;

    if (!estoque_ler_codigo(peca.codigo, sizeof(peca.codigo))) return;
    resultado = util_ler_texto_obrigatorio("Nome: ", peca.nome,
                                            sizeof(peca.nome));
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    resultado = util_ler_inteiro("Estoque inicial: ", 0, 100000,
                                 &peca.quantidade);
    if (resultado == RES_OK) {
        resultado = util_ler_inteiro("Estoque mínimo: ", 0, 100000,
                                     &peca.estoque_minimo);
    }
    if (resultado == RES_OK) resultado = peca_cadastrar(db, &peca, &id_novo);
    if (resultado == RES_OK) printf("[OK] Peça cadastrada (ID %d).\n", id_novo);
    else util_erro(util_texto_resultado(resultado));
}

static void estoque_imprimir_cabecalho(void)
{
    util_imprimir_coluna("Código", 16);
    util_imprimir_coluna("Nome", 28);
    util_imprimir_coluna("Quantidade", 12);
    util_imprimir_coluna("Mínimo", 10);
    util_imprimir_coluna("Situação", 10);
    putchar('\n');
}

static void estoque_imprimir_peca(const Peca *peca)
{
    char quantidade[16];
    char minimo[16];

    snprintf(quantidade, sizeof(quantidade), "%d", peca->quantidade);
    snprintf(minimo, sizeof(minimo), "%d", peca->estoque_minimo);
    util_imprimir_coluna(peca->codigo, 16);
    util_imprimir_coluna(peca->nome, 28);
    util_imprimir_coluna(quantidade, 12);
    util_imprimir_coluna(minimo, 10);
    util_imprimir_coluna(peca->quantidade <= peca->estoque_minimo
                             ? "BAIXO" : "OK",
                         10);
    putchar('\n');
}

static void estoque_listar_tela(sqlite3 *db, int baixo)
{
    Peca *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = baixo
        ? peca_listar_estoque_baixo(db, &lista, &quantidade)
        : peca_listar(db, &lista, &quantidade);

    if (resultado != RES_OK) util_erro(util_texto_resultado(resultado));
    else if (quantidade == 0) puts("Nenhuma peça encontrada.");
    else {
        estoque_imprimir_cabecalho();
        for (i = 0; i < quantidade; i++) estoque_imprimir_peca(&lista[i]);
    }
    free(lista);
}

static void estoque_consultar_tela(sqlite3 *db)
{
    Peca peca;

    if (!estoque_buscar_tela(db, &peca)) return;
    printf("Nome: %s\nQuantidade: %d\nEstoque mínimo: %d\nSituação: %s\n",
           peca.nome, peca.quantidade, peca.estoque_minimo,
           peca.quantidade <= peca.estoque_minimo ? "BAIXO" : "OK");
}

static int estoque_confirmar(void)
{
    int confirmado = 0;
    Resultado resultado = util_ler_confirmacao("Confirmar operação? (S/N): ",
                                                &confirmado);

    if (resultado != RES_OK) util_erro(util_texto_resultado(resultado));
    return resultado == RES_OK && confirmado;
}

static void estoque_movimentar_tela(sqlite3 *db, int entrada)
{
    Peca peca;
    char motivo[201] = "";
    int quantidade;
    Resultado resultado;

    if (!estoque_buscar_tela(db, &peca)) return;
    printf("%s - quantidade atual: %d\n", peca.nome, peca.quantidade);
    resultado = util_ler_inteiro("Quantidade: ", 1, 100000, &quantidade);
    if (resultado == RES_OK) {
        resultado = entrada
            ? util_ler_linha("Motivo (opcional): ", motivo, sizeof(motivo))
            : util_ler_texto_obrigatorio("Motivo: ", motivo, sizeof(motivo));
    }
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    if (!estoque_confirmar()) return;
    resultado = entrada
        ? peca_registrar_entrada(db, peca.id, quantidade,
                                 motivo[0] == '\0' ? NULL : motivo)
        : peca_registrar_saida(db, peca.id, quantidade, motivo);
    if (resultado == RES_OK) {
        resultado = peca_buscar_por_id(db, peca.id, &peca);
        if (resultado == RES_OK) printf("[OK] Nova quantidade: %d.\n", peca.quantidade);
    }
    if (resultado != RES_OK) util_erro(util_texto_resultado(resultado));
}

static void estoque_imprimir_movimento(const MovimentoEstoque *movimento)
{
    char data[20];
    char quantidade[16];
    char ordem[16];

    util_formatar_data(movimento->data_hora, data, sizeof(data));
    snprintf(quantidade, sizeof(quantidade), "%d", movimento->quantidade);
    if (movimento->ordem_id == 0) snprintf(ordem, sizeof(ordem), "-");
    else snprintf(ordem, sizeof(ordem), "%d", movimento->ordem_id);
    util_imprimir_coluna(data, 19);
    util_imprimir_coluna(movimento->tipo, 8);
    util_imprimir_coluna(quantidade, 10);
    util_imprimir_coluna(ordem, 8);
    util_imprimir_coluna(movimento->motivo, 30);
    putchar('\n');
}

static void estoque_movimentacoes_tela(sqlite3 *db)
{
    Peca peca;
    MovimentoEstoque *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado;

    if (!estoque_buscar_tela(db, &peca)) return;
    resultado = estoque_listar_movimentacoes(db, peca.id, &lista, &quantidade);
    if (resultado != RES_OK) util_erro(util_texto_resultado(resultado));
    else if (quantidade == 0) puts("Nenhuma movimentação encontrada.");
    else {
        util_imprimir_coluna("Data", 19);
        util_imprimir_coluna("Tipo", 8);
        util_imprimir_coluna("Quantidade", 10);
        util_imprimir_coluna("Ordem", 8);
        util_imprimir_coluna("Motivo", 30);
        putchar('\n');
        for (i = 0; i < quantidade; i++) {
            estoque_imprimir_movimento(&lista[i]);
        }
    }
    free(lista);
}

void menu_estoque(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("CONTROLE DE ESTOQUE");
        puts("1 - Cadastrar peça\n2 - Listar peças");
        puts("3 - Consultar disponibilidade\n4 - Registrar entrada");
        puts("5 - Registrar saída\n6 - Estoque baixo");
        puts("7 - Movimentações de uma peça\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 7, &opcao);
        if (resultado != RES_OK) return;
        switch (opcao) {
            case 0: return;
            case 1: estoque_cadastrar_tela(db); break;
            case 2: estoque_listar_tela(db, 0); break;
            case 3: estoque_consultar_tela(db); break;
            case 4: estoque_movimentar_tela(db, 1); break;
            case 5: estoque_movimentar_tela(db, 0); break;
            case 6: estoque_listar_tela(db, 1); break;
            case 7: estoque_movimentacoes_tela(db); break;
            default: break;
        }
    }
}
