#include "pecas.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_pecas.sqlite"

static int contar_movimentos(sqlite3 *db)
{
    sqlite3_stmt *consulta = NULL;
    int total = -1;

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM movimentacoes_estoque;",
                           -1, &consulta, NULL) == SQLITE_OK &&
        sqlite3_step(consulta) == SQLITE_ROW) {
        total = sqlite3_column_int(consulta, 0);
    }
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) return -1;
    return total;
}

static int inserir_peca(sqlite3 *db, const char *codigo, const char *nome,
                        int quantidade, int minimo, int *id)
{
    Peca peca = {0};

    snprintf(peca.codigo, sizeof(peca.codigo), "%s", codigo);
    snprintf(peca.nome, sizeof(peca.nome), "%s", nome);
    peca.quantidade = quantidade;
    peca.estoque_minimo = minimo;
    return peca_cadastrar(db, &peca, id) == RES_OK;
}

static void testar_duplicados(sqlite3 *db, Peca *peca)
{
    int id_novo = 0;

    snprintf(peca->codigo, sizeof(peca->codigo), "%s", "SSD480");
    TESTE_VERIFICAR(peca_cadastrar(db, peca, &id_novo) == RES_DUPLICADO,
                    "rejeitar código duplicado em maiúsculas");
    snprintf(peca->codigo, sizeof(peca->codigo), "%s", "ssd480");
    TESTE_VERIFICAR(peca_cadastrar(db, peca, &id_novo) == RES_DUPLICADO,
                    "rejeitar código duplicado em minúsculas");
}

static void testar_cadastro(sqlite3 *db, int *ssd_id, int *zero_id,
                            int *mouse_id)
{
    Peca peca = {0};
    MovimentoEstoque *movimentos = NULL;
    int quantidade = 0;

    snprintf(peca.codigo, sizeof(peca.codigo), "%s", "ssd480");
    snprintf(peca.nome, sizeof(peca.nome), "%s", "SSD 480 GB");
    peca.quantidade = 10;
    peca.estoque_minimo = 3;
    TESTE_VERIFICAR(peca_cadastrar(db, &peca, ssd_id) == RES_OK,
                    "cadastrar peça com estoque inicial");
    TESTE_VERIFICAR(peca_buscar_por_id(db, *ssd_id, &peca) == RES_OK &&
                    strcmp(peca.codigo, "SSD480") == 0,
                    "normalizar código para maiúsculas");
    TESTE_VERIFICAR(estoque_listar_movimentacoes(db, *ssd_id, &movimentos,
                                                  &quantidade) == RES_OK &&
                    quantidade == 1 &&
                    strcmp(movimentos[0].tipo, "ENTRADA") == 0 &&
                    movimentos[0].quantidade == 10 &&
                    strcmp(movimentos[0].motivo, "Estoque inicial") == 0,
                    "registrar movimento do estoque inicial");
    free(movimentos);
    TESTE_VERIFICAR(inserir_peca(db, "zero", "Sem estoque", 0, 0, zero_id),
                    "cadastrar peça sem estoque inicial");
    movimentos = NULL;
    quantidade = -1;
    TESTE_VERIFICAR(estoque_listar_movimentacoes(db, *zero_id, &movimentos,
                                                  &quantidade) == RES_OK &&
                    quantidade == 0 && movimentos == NULL,
                    "estoque inicial zero não gera movimento");
    TESTE_VERIFICAR(inserir_peca(db, "mouse", "Mouse", 4, 2, mouse_id),
                    "cadastrar peça acima do mínimo");
    testar_duplicados(db, &peca);
}

static void testar_validacoes(sqlite3 *db)
{
    Peca peca = {0};
    int id_novo = 0;

    snprintf(peca.nome, sizeof(peca.nome), "%s", "Peça");
    peca.quantidade = 1;
    TESTE_VERIFICAR(peca_validar(&peca) == RES_INVALIDO,
                    "rejeitar código vazio");
    snprintf(peca.codigo, sizeof(peca.codigo), "%s", "VALIDA");
    peca.nome[0] = '\0';
    TESTE_VERIFICAR(peca_validar(&peca) == RES_INVALIDO,
                    "rejeitar nome vazio");
    snprintf(peca.nome, sizeof(peca.nome), "%s", "Peça");
    peca.quantidade = -1;
    TESTE_VERIFICAR(peca_validar(&peca) == RES_INVALIDO,
                    "rejeitar quantidade negativa no cadastro");
    peca.quantidade = 0;
    peca.estoque_minimo = -1;
    TESTE_VERIFICAR(peca_validar(&peca) == RES_INVALIDO,
                    "rejeitar mínimo negativo");
    peca.estoque_minimo = 0;
    peca.quantidade = 100001;
    TESTE_VERIFICAR(peca_validar(&peca) == RES_INVALIDO,
                    "rejeitar estoque inicial acima do limite");
    TESTE_VERIFICAR(peca_cadastrar(db, &peca, &id_novo) == RES_INVALIDO,
                    "cadastro inválido não grava peça");
}

static void testar_buscas(sqlite3 *db, int ssd_id)
{
    Peca peca;

    TESTE_VERIFICAR(peca_buscar_por_id(db, ssd_id, &peca) == RES_OK &&
                    strcmp(peca.nome, "SSD 480 GB") == 0,
                    "buscar peça por id");
    TESTE_VERIFICAR(peca_buscar_por_codigo(db, "ssd480", &peca) == RES_OK &&
                    peca.id == ssd_id,
                    "buscar código sem diferenciar caixa");
    TESTE_VERIFICAR(peca_buscar_por_id(db, 999999, &peca) ==
                        RES_NAO_ENCONTRADO &&
                    peca_buscar_por_codigo(db, "INEXISTENTE", &peca) ==
                        RES_NAO_ENCONTRADO,
                    "rejeitar peças inexistentes");
}

static void testar_entrada(sqlite3 *db, int ssd_id)
{
    Peca peca;

    TESTE_VERIFICAR(peca_registrar_entrada(db, ssd_id, 5, "Reposição") ==
                        RES_OK &&
                    peca_buscar_por_id(db, ssd_id, &peca) == RES_OK &&
                    peca.quantidade == 15,
                    "registrar entrada e atualizar quantidade");
    TESTE_VERIFICAR(peca_registrar_entrada(db, ssd_id, 0, NULL) == RES_INVALIDO &&
                    peca_registrar_entrada(db, ssd_id, -1, NULL) == RES_INVALIDO &&
                    peca_registrar_entrada(db, ssd_id, 100001, NULL) ==
                        RES_INVALIDO,
                    "rejeitar quantidades de entrada inválidas");
    TESTE_VERIFICAR(peca_registrar_entrada(db, 999999, 1, NULL) ==
                        RES_NAO_ENCONTRADO,
                    "entrada em peça inexistente");
}

static void testar_saida(sqlite3 *db, int ssd_id)
{
    Peca peca;
    int movimentos_antes = contar_movimentos(db);

    TESTE_VERIFICAR(peca_registrar_saida(db, ssd_id, 1, "") == RES_INVALIDO,
                    "saída manual exige motivo");
    TESTE_VERIFICAR(peca_registrar_saida(db, ssd_id, 0, "Inválida") ==
                        RES_INVALIDO &&
                    peca_registrar_saida(db, ssd_id, 100001, "Inválida") ==
                        RES_INVALIDO,
                    "rejeitar quantidades de saída inválidas");
    TESTE_VERIFICAR(peca_registrar_saida(db, ssd_id, 3, "Uso") == RES_OK &&
                    peca_buscar_por_id(db, ssd_id, &peca) == RES_OK &&
                    peca.quantidade == 12,
                    "registrar saída e atualizar quantidade");
    movimentos_antes = contar_movimentos(db);
    TESTE_VERIFICAR(peca_registrar_saida(db, ssd_id, 13, "Excesso") ==
                        RES_ESTOQUE_INSUFICIENTE,
                    "rejeitar saída acima do disponível");
    TESTE_VERIFICAR(peca_buscar_por_id(db, ssd_id, &peca) == RES_OK &&
                    peca.quantidade == 12 &&
                    contar_movimentos(db) == movimentos_antes,
                    "saída insuficiente não altera estoque nem histórico");
    TESTE_VERIFICAR(peca_registrar_saida(db, ssd_id, 12, "Baixa total") ==
                        RES_OK &&
                    peca_buscar_por_id(db, ssd_id, &peca) == RES_OK &&
                    peca.quantidade == 0,
                    "saída exata deixa estoque zerado");
    TESTE_VERIFICAR(peca_registrar_saida(db, 999999, 1, "Peça ausente") ==
                        RES_NAO_ENCONTRADO,
                    "saída de peça inexistente");
}

static void testar_listas(sqlite3 *db, int ssd_id)
{
    Peca *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = peca_listar(db, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 3,
                    "listar todas as peças");
    if (quantidade == 3) {
        TESTE_VERIFICAR(strcmp(lista[0].codigo, "MOUSE") == 0 &&
                        strcmp(lista[1].codigo, "SSD480") == 0 &&
                        strcmp(lista[2].codigo, "ZERO") == 0,
                        "peças ordenadas por código");
    }
    free(lista);
    lista = NULL;
    quantidade = 0;
    resultado = peca_listar_estoque_baixo(db, &lista, &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 2,
                    "listar estoque baixo incluindo igual ao mínimo");
    if (quantidade == 2) {
        for (i = 0; i < quantidade; i++) {
            TESTE_VERIFICAR(lista[i].quantidade <= lista[i].estoque_minimo,
                            "estoque baixo respeita mínimo");
        }
        TESTE_VERIFICAR(lista[0].id == ssd_id,
                        "estoque baixo inclui peça abaixo do mínimo");
    }
    free(lista);
}

static void testar_movimentacoes(sqlite3 *db, int ssd_id)
{
    MovimentoEstoque *lista = NULL;
    int quantidade = 0;
    Resultado resultado = estoque_listar_movimentacoes(db, ssd_id, &lista,
                                                        &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 4,
                    "listar movimentos da peça");
    if (quantidade == 4) {
        TESTE_VERIFICAR(strcmp(lista[0].tipo, "ENTRADA") == 0 &&
                        lista[0].quantidade == 10 &&
                        strcmp(lista[1].tipo, "ENTRADA") == 0 &&
                        strcmp(lista[2].tipo, "SAIDA") == 0 &&
                        strcmp(lista[3].tipo, "SAIDA") == 0,
                        "movimentações em ordem cronológica");
    }
    free(lista);
    lista = NULL;
    quantidade = 0;
    resultado = estoque_listar_movimentacoes(db, 0, &lista, &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 5,
                    "listar movimentações de todas as peças");
    free(lista);
}

static void testar_saida_em_transacao(sqlite3 *db, int ssd_id)
{
    Peca peca;
    MovimentoEstoque *lista = NULL;
    int quantidade = 0;
    Resultado resultado;
    int total_antes = contar_movimentos(db);

    TESTE_VERIFICAR(peca_registrar_entrada(db, ssd_id, 4, "Teste") == RES_OK,
                    "preparar estoque para transação manual");
    total_antes = contar_movimentos(db);
    resultado = db_iniciar_transacao(db);
    if (resultado == RES_OK) {
        resultado = peca_saida_em_transacao(db, ssd_id, 1, 0, "Temporária");
        if (db_desfazer(db) != RES_OK) resultado = RES_ERRO_BANCO;
    }
    TESTE_VERIFICAR(resultado == RES_OK,
                    "saída em transação não confirma sozinha");
    TESTE_VERIFICAR(peca_buscar_por_id(db, ssd_id, &peca) == RES_OK &&
                    peca.quantidade == 4 &&
                    contar_movimentos(db) == total_antes,
                    "rollback manual restaura quantidade e movimento");
    TESTE_VERIFICAR(estoque_listar_movimentacoes(db, ssd_id, &lista,
                                                  &quantidade) == RES_OK &&
                    quantidade == 5,
                    "movimentos permanecem após rollback manual");
    free(lista);
}

static void testar_rollback_saida(sqlite3 *db, int ssd_id)
{
    Peca peca;
    const char *trigger =
        "CREATE TRIGGER falha_saida BEFORE INSERT ON movimentacoes_estoque "
        "WHEN NEW.tipo = 'SAIDA' "
        "BEGIN SELECT RAISE(ABORT, 'falha forcada'); END;";
    int codigo = sqlite3_exec(db, trigger, NULL, NULL, NULL);

    TESTE_VERIFICAR(codigo == SQLITE_OK, "criar trigger de falha de saída");
    if (codigo != SQLITE_OK) return;
    TESTE_VERIFICAR(peca_registrar_saida(db, ssd_id, 1, "Falha") != RES_OK &&
                    peca_buscar_por_id(db, ssd_id, &peca) == RES_OK &&
                    peca.quantidade == 4,
                    "rollback restaura estoque após erro no movimento");
    codigo = sqlite3_exec(db, "DROP TRIGGER falha_saida;", NULL, NULL, NULL);
    TESTE_VERIFICAR(codigo == SQLITE_OK, "remover trigger de falha");
    TESTE_VERIFICAR(peca_registrar_saida(db, ssd_id, 1, "Saída final") ==
                        RES_OK,
                    "saída funciona após rollback");
}

static void testar_persistencia(sqlite3 **db, int ssd_id)
{
    Peca peca;
    MovimentoEstoque *lista = NULL;
    int quantidade = 0;

    db_fechar(*db);
    *db = NULL;
    TESTE_VERIFICAR(db_abrir(db, CAMINHO_BANCO) == RES_OK,
                    "reabrir banco de peças");
    TESTE_VERIFICAR(peca_buscar_por_id(*db, ssd_id, &peca) == RES_OK &&
                    peca.quantidade == 3 &&
                    strcmp(peca.codigo, "SSD480") == 0,
                    "peça e quantidade persistem");
    TESTE_VERIFICAR(estoque_listar_movimentacoes(*db, ssd_id, &lista,
                                                  &quantidade) == RES_OK &&
                    quantidade == 6 &&
                    strcmp(lista[5].motivo, "Saída final") == 0,
                    "movimentações persistem");
    free(lista);
}

int main(void)
{
    sqlite3 *db = NULL;
    int ssd_id = 0;
    int zero_id = 0;
    int mouse_id = 0;

    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        TESTE_VERIFICAR(0, "remover banco anterior");
        return teste_codigo_saida();
    }
    if (db_abrir(&db, CAMINHO_BANCO) != RES_OK) {
        TESTE_VERIFICAR(0, "abrir banco de peças");
        return teste_codigo_saida();
    }
    testar_cadastro(db, &ssd_id, &zero_id, &mouse_id);
    testar_validacoes(db);
    testar_buscas(db, ssd_id);
    testar_entrada(db, ssd_id);
    testar_saida(db, ssd_id);
    testar_listas(db, ssd_id);
    testar_movimentacoes(db, ssd_id);
    testar_saida_em_transacao(db, ssd_id);
    testar_rollback_saida(db, ssd_id);
    testar_persistencia(&db, ssd_id);
    db_fechar(db);
    return teste_codigo_saida();
}
