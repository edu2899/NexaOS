#include "tecnicos.h"

#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void tecnico_copiar_coluna(char *destino, size_t tamanho,
                                  sqlite3_stmt *consulta, int coluna)
{
    const unsigned char *texto = sqlite3_column_text(consulta, coluna);

    snprintf(destino, tamanho, "%s", texto != NULL ? (const char *)texto : "");
}

static void tecnico_ler_colunas(sqlite3_stmt *consulta, Tecnico *tecnico)
{
    tecnico->id = sqlite3_column_int(consulta, 0);
    tecnico_copiar_coluna(tecnico->nome, sizeof(tecnico->nome), consulta, 1);
    tecnico_copiar_coluna(tecnico->especialidade,
                          sizeof(tecnico->especialidade), consulta, 2);
}

static Resultado tecnico_finalizar(sqlite3 *db, sqlite3_stmt *consulta,
                                   Resultado resultado)
{
    int codigo = sqlite3_finalize(consulta);

    if (codigo != SQLITE_OK && resultado == RES_OK) {
        return db_traduzir_erro(db);
    }
    return resultado;
}

Resultado tecnico_validar(const Tecnico *tecnico)
{
    if (tecnico == NULL || tecnico->nome[0] == '\0' ||
        tecnico->especialidade[0] == '\0') {
        return RES_INVALIDO;
    }
    return RES_OK;
}

Resultado tecnico_inserir(sqlite3 *db, const Tecnico *tecnico, int *id_novo)
{
    static const char sql[] =
        "INSERT INTO tecnicos (nome, especialidade) VALUES (?1, ?2);";
    sqlite3_stmt *consulta = NULL;
    int codigo;
    Resultado resultado = tecnico_validar(tecnico);

    if (db == NULL || id_novo == NULL || resultado != RES_OK) {
        return resultado != RES_OK ? resultado : RES_INVALIDO;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }

    codigo = sqlite3_bind_text(consulta, 1, tecnico->nome, -1,
                               SQLITE_TRANSIENT);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_bind_text(consulta, 2, tecnico->especialidade, -1,
                                   SQLITE_TRANSIENT);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    if (codigo != SQLITE_DONE) {
        return tecnico_finalizar(db, consulta, db_traduzir_erro(db));
    }

    *id_novo = (int)sqlite3_last_insert_rowid(db);
    return tecnico_finalizar(db, consulta, RES_OK);
}

Resultado tecnico_buscar_por_id(sqlite3 *db, int id, Tecnico *saida)
{
    static const char sql[] =
        "SELECT id, nome, especialidade FROM tecnicos WHERE id = ?1;";
    sqlite3_stmt *consulta = NULL;
    int codigo;

    if (db == NULL || saida == NULL || id <= 0) {
        return RES_INVALIDO;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    codigo = sqlite3_bind_int(consulta, 1, id);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    if (codigo == SQLITE_ROW) {
        tecnico_ler_colunas(consulta, saida);
        return tecnico_finalizar(db, consulta, RES_OK);
    }
    if (codigo == SQLITE_DONE) {
        return tecnico_finalizar(db, consulta, RES_NAO_ENCONTRADO);
    }
    return tecnico_finalizar(db, consulta, db_traduzir_erro(db));
}

static Resultado tecnico_adicionar_linha(sqlite3_stmt *consulta,
                                         Tecnico **tecnicos,
                                         size_t *capacidade, int quantidade)
{
    if ((size_t)quantidade == *capacidade) {
        size_t nova_capacidade;
        Tecnico *temporario;

        if (*capacidade > (size_t)-1 / 2 ||
            *capacidade > (size_t)INT_MAX / 2) {
            return RES_ERRO_MEMORIA;
        }
        nova_capacidade = *capacidade * 2;
        if (nova_capacidade > (size_t)-1 / sizeof(**tecnicos)) {
            return RES_ERRO_MEMORIA;
        }
        temporario = realloc(*tecnicos, nova_capacidade * sizeof(**tecnicos));
        if (temporario == NULL) {
            return RES_ERRO_MEMORIA;
        }
        *tecnicos = temporario;
        *capacidade = nova_capacidade;
    }

    tecnico_ler_colunas(consulta, &(*tecnicos)[quantidade]);
    return RES_OK;
}

Resultado tecnico_listar(sqlite3 *db, Tecnico **lista, int *quantidade)
{
    static const char sql[] =
        "SELECT id, nome, especialidade FROM tecnicos ORDER BY id;";
    sqlite3_stmt *consulta = NULL;
    Tecnico *tecnicos;
    size_t capacidade = 8;
    int total = 0;
    int codigo;
    Resultado resultado = RES_OK;

    if (db == NULL || lista == NULL || quantidade == NULL) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;
    tecnicos = malloc(capacidade * sizeof(*tecnicos));
    if (tecnicos == NULL) {
        return RES_ERRO_MEMORIA;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        free(tecnicos);
        return db_traduzir_erro(db);
    }

    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        if (total == INT_MAX) {
            resultado = RES_ERRO_MEMORIA;
            break;
        }
        resultado = tecnico_adicionar_linha(consulta, &tecnicos, &capacidade,
                                            total);
        if (resultado != RES_OK) {
            break;
        }
        total++;
    }
    if (codigo != SQLITE_DONE && resultado == RES_OK) {
        resultado = db_traduzir_erro(db);
    }
    resultado = tecnico_finalizar(db, consulta, resultado);
    if (resultado != RES_OK) {
        free(tecnicos);
        return resultado;
    }
    if (total == 0) {
        free(tecnicos);
        return RES_OK;
    }

    *lista = tecnicos;
    *quantidade = total;
    return RES_OK;
}

static int tecnico_ler_id(int *id)
{
    Resultado resultado = util_ler_inteiro("ID do técnico: ", 1, INT_MAX, id);

    if (resultado != RES_OK) {
        if (resultado != RES_ERRO_ARQUIVO) {
            util_erro(util_texto_resultado(resultado));
        }
        return 0;
    }
    return 1;
}

static void tecnico_cadastrar(sqlite3 *db)
{
    Tecnico tecnico = {0};
    int id_novo;
    Resultado resultado = util_ler_texto_obrigatorio(
        "Nome: ", tecnico.nome, sizeof(tecnico.nome));

    if (resultado == RES_OK) {
        resultado = util_ler_texto_obrigatorio(
            "Especialidade: ", tecnico.especialidade,
            sizeof(tecnico.especialidade));
    }
    if (resultado == RES_OK) {
        resultado = tecnico_inserir(db, &tecnico, &id_novo);
    }
    if (resultado == RES_OK) {
        printf("[OK] Técnico cadastrado com ID %d.\n", id_novo);
    } else {
        util_erro(util_texto_resultado(resultado));
    }
}

static void tecnico_listar_tela(sqlite3 *db)
{
    Tecnico *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = tecnico_listar(db, &lista, &quantidade);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhum técnico cadastrado.");
    } else {
        printf("%-6s %-30s %-30s\n", "ID", "Nome", "Especialidade");
        for (i = 0; i < quantidade; i++) {
            printf("%-6d %-30.30s %-30.30s\n", lista[i].id, lista[i].nome,
                   lista[i].especialidade);
        }
    }
    free(lista);
}

static void tecnico_consultar(sqlite3 *db)
{
    Tecnico tecnico;
    int id;
    Resultado resultado;

    if (!tecnico_ler_id(&id)) {
        return;
    }
    resultado = tecnico_buscar_por_id(db, id, &tecnico);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    printf("ID: %d\nNome: %s\nEspecialidade: %s\n", tecnico.id,
           tecnico.nome, tecnico.especialidade);
}

void menu_tecnicos(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("TÉCNICOS");
        puts("1 - Cadastrar\n2 - Listar\n3 - Consultar por ID\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 3, &opcao);
        if (resultado != RES_OK) {
            return;
        }
        switch (opcao) {
            case 0:
                return;
            case 1:
                tecnico_cadastrar(db);
                break;
            case 2:
                tecnico_listar_tela(db);
                break;
            case 3:
                tecnico_consultar(db);
                break;
            default:
                break;
        }
    }
}
