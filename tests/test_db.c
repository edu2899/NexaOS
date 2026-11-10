#include "db.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static int tabela_existe(sqlite3 *db, const char *nome)
{
    sqlite3_stmt *consulta = NULL;
    int existe = 0;

    if (sqlite3_prepare_v2(db,
            "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1;",
            -1, &consulta, NULL) == SQLITE_OK &&
        sqlite3_bind_text(consulta, 1, nome, -1, SQLITE_STATIC) == SQLITE_OK &&
        sqlite3_step(consulta) == SQLITE_ROW) {
        existe = 1;
    }

    if (sqlite3_finalize(consulta) != SQLITE_OK) {
        return 0;
    }
    return existe;
}

static int valor_foreign_keys(sqlite3 *db)
{
    sqlite3_stmt *consulta = NULL;
    int valor = -1;

    if (sqlite3_prepare_v2(db, "PRAGMA foreign_keys;", -1, &consulta, NULL) ==
            SQLITE_OK &&
        sqlite3_step(consulta) == SQLITE_ROW) {
        valor = sqlite3_column_int(consulta, 0);
    }

    if (sqlite3_finalize(consulta) != SQLITE_OK) {
        return -1;
    }
    return valor;
}

static int cliente_existe(sqlite3 *db)
{
    sqlite3_stmt *consulta = NULL;
    int existe = 0;

    if (sqlite3_prepare_v2(db,
            "SELECT 1 FROM clientes WHERE documento = '12345678901';",
            -1, &consulta, NULL) == SQLITE_OK &&
        sqlite3_step(consulta) == SQLITE_ROW) {
        existe = 1;
    }

    if (sqlite3_finalize(consulta) != SQLITE_OK) {
        return 0;
    }
    return existe;
}

int main(void)
{
    vazamento_iniciar();
    static const char *tabelas[] = {
        "clientes", "equipamentos", "tecnicos", "ordens_servico",
        "pecas", "itens_ordem", "historico_ordens", "movimentacoes_estoque"
    };
    sqlite3 *db = NULL;
    char *mensagem = NULL;
    size_t i;
    Resultado resultado;

    if (remove("build\\test_db.sqlite") != 0 && errno != ENOENT) {
        perror("build\\test_db.sqlite");
        TESTE_VERIFICAR(0, "preparar arquivo temporario do banco");
        return teste_codigo_saida();
    }

    resultado = db_abrir(&db, "build\\test_db.sqlite");
    TESTE_VERIFICAR(resultado == RES_OK, "abrir banco e aplicar esquema");
    if (resultado != RES_OK) {
        return teste_codigo_saida();
    }

    for (i = 0; i < sizeof(tabelas) / sizeof(tabelas[0]); i++) {
        TESTE_VERIFICAR(tabela_existe(db, tabelas[i]), tabelas[i]);
    }

    TESTE_VERIFICAR(valor_foreign_keys(db) == 1, "foreign_keys ativado");

    resultado = (Resultado)sqlite3_exec(db,
        "INSERT INTO equipamentos "
        "(cliente_id, tipo, marca, modelo, numero_serie) "
        "VALUES (999999, 'COMPUTADOR', 'Marca', 'Modelo', 'SERIE-TESTE');",
        NULL, NULL, &mensagem);
    TESTE_VERIFICAR(resultado != SQLITE_OK, "rejeitar equipamento sem cliente");
    TESTE_VERIFICAR(db_traduzir_erro(db) == RES_INVALIDO,
                    "traduzir erro de chave estrangeira");
    sqlite3_free(mensagem);
    mensagem = NULL;

    resultado = (Resultado)sqlite3_exec(db,
        "INSERT INTO clientes (nome, documento, telefone) "
        "VALUES ('Cliente teste', '12345678901', '11999999999');",
        NULL, NULL, &mensagem);
    TESTE_VERIFICAR(resultado == RES_OK, "inserir cliente para persistencia");
    sqlite3_free(mensagem);
    mensagem = NULL;

    db_fechar(db);
    db = NULL;
    resultado = db_abrir(&db, "build\\test_db.sqlite");
    TESTE_VERIFICAR(resultado == RES_OK, "reabrir banco persistido");
    if (resultado == RES_OK) {
        TESTE_VERIFICAR(cliente_existe(db), "cliente permanece apos reabrir");

        resultado = (Resultado)sqlite3_exec(db,
            "INSERT INTO pecas (codigo, nome, quantidade) "
            "VALUES ('P-TESTE', 'Peca teste', 1);"
            "UPDATE pecas SET quantidade = -1 WHERE codigo = 'P-TESTE';",
            NULL, NULL, &mensagem);
        TESTE_VERIFICAR(resultado != SQLITE_OK, "rejeitar quantidade negativa");
        TESTE_VERIFICAR(db_traduzir_erro(db) == RES_INVALIDO,
                        "traduzir erro de verificacao CHECK");
        sqlite3_free(mensagem);
        mensagem = NULL;
    }

    db_fechar(db);
    return teste_codigo_saida();
}
