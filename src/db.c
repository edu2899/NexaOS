#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Resultado db_executar_sql(sqlite3 *db, const char *sql)
{
    char *mensagem = NULL;
    int codigo;

    if (db == NULL || sql == NULL) {
        return RES_INVALIDO;
    }

    codigo = sqlite3_exec(db, sql, NULL, NULL, &mensagem);

    if (codigo != SQLITE_OK) {
        fprintf(stderr, "Erro SQLite: %s\n",
                mensagem != NULL ? mensagem : sqlite3_errmsg(db));
        sqlite3_free(mensagem);
        return db_traduzir_erro(db);
    }

    sqlite3_free(mensagem);
    return RES_OK;
}

Resultado db_executar_script(sqlite3 *db, const char *arquivo)
{
    FILE *arq;
    long tamanho;
    char *conteudo;
    size_t lidos;
    Resultado resultado;

    if (db == NULL || arquivo == NULL) {
        return RES_INVALIDO;
    }

    arq = fopen(arquivo, "rb");
    if (arq == NULL) {
        if (strcmp(arquivo, "database/schema.sql") == 0) {
            fprintf(stderr, "Execute o programa a partir da pasta raiz do projeto.\n");
        } else {
            perror(arquivo);
        }
        return RES_ERRO_ARQUIVO;
    }

    if (fseek(arq, 0, SEEK_END) != 0 || (tamanho = ftell(arq)) < 0 ||
        fseek(arq, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Erro ao ler o arquivo %s.\n", arquivo);
        if (fclose(arq) != 0) {
            perror("fclose");
        }
        return RES_ERRO_ARQUIVO;
    }

    conteudo = malloc((size_t)tamanho + 1);
    if (conteudo == NULL) {
        if (fclose(arq) != 0) {
            perror("fclose");
        }
        return RES_ERRO_MEMORIA;
    }

    lidos = fread(conteudo, 1, (size_t)tamanho, arq);
    if (lidos != (size_t)tamanho || ferror(arq)) {
        fprintf(stderr, "Erro ao ler o arquivo %s.\n", arquivo);
        free(conteudo);
        if (fclose(arq) != 0) {
            perror("fclose");
        }
        return RES_ERRO_ARQUIVO;
    }

    conteudo[tamanho] = '\0';
    if (fclose(arq) != 0) {
        fprintf(stderr, "Erro ao fechar o arquivo %s.\n", arquivo);
        free(conteudo);
        return RES_ERRO_ARQUIVO;
    }

    resultado = db_executar_sql(db, conteudo);
    free(conteudo);
    return resultado;
}

Resultado db_abrir(sqlite3 **db, const char *caminho)
{
    sqlite3_stmt *consulta = NULL;
    int codigo;
    Resultado resultado;

    if (db == NULL || caminho == NULL) {
        return RES_INVALIDO;
    }
    *db = NULL;

    codigo = sqlite3_open(caminho, db);
    if (codigo != SQLITE_OK) {
        fprintf(stderr, "Erro ao abrir o banco: %s\n",
                *db != NULL ? sqlite3_errmsg(*db) : "falha de memoria");
        db_fechar(*db);
        *db = NULL;
        return RES_ERRO_BANCO;
    }

    if (sqlite3_extended_result_codes(*db, 1) != SQLITE_OK) {
        resultado = RES_ERRO_BANCO;
        goto falha;
    }

    resultado = db_executar_sql(*db, "PRAGMA foreign_keys = ON;");
    if (resultado != RES_OK) {
        goto falha;
    }

    codigo = sqlite3_prepare_v2(*db, "PRAGMA foreign_keys;", -1, &consulta, NULL);
    if (codigo != SQLITE_OK) {
        fprintf(stderr, "Erro SQLite: %s\n", sqlite3_errmsg(*db));
        resultado = db_traduzir_erro(*db);
        goto falha;
    }

    codigo = sqlite3_step(consulta);
    if (codigo != SQLITE_ROW || sqlite3_column_int(consulta, 0) != 1) {
        fprintf(stderr, "Erro SQLite: nao foi possivel ativar as chaves estrangeiras.\n");
        codigo = sqlite3_finalize(consulta);
        consulta = NULL;
        if (codigo != SQLITE_OK) {
            fprintf(stderr, "Erro SQLite: %s\n", sqlite3_errmsg(*db));
        }
        resultado = RES_ERRO_BANCO;
        goto falha;
    }

    codigo = sqlite3_finalize(consulta);
    consulta = NULL;
    if (codigo != SQLITE_OK) {
        fprintf(stderr, "Erro SQLite: %s\n", sqlite3_errmsg(*db));
        resultado = db_traduzir_erro(*db);
        goto falha;
    }

    resultado = db_executar_script(*db, "database/schema.sql");
    if (resultado != RES_OK) {
        goto falha;
    }

    return RES_OK;

falha:
    if (consulta != NULL) {
        codigo = sqlite3_finalize(consulta);
        if (codigo != SQLITE_OK) {
            fprintf(stderr, "Erro SQLite: %s\n", sqlite3_errmsg(*db));
        }
    }
    db_fechar(*db);
    *db = NULL;
    return resultado;
}

void db_fechar(sqlite3 *db)
{
    if (db != NULL) {
        int codigo = sqlite3_close(db);
        if (codigo != SQLITE_OK) {
            fprintf(stderr, "Erro ao fechar o banco: %s\n", sqlite3_errstr(codigo));
        }
    }
}

Resultado db_iniciar_transacao(sqlite3 *db)
{
    return db_executar_sql(db, "BEGIN IMMEDIATE;");
}

Resultado db_confirmar(sqlite3 *db)
{
    return db_executar_sql(db, "COMMIT;");
}

Resultado db_desfazer(sqlite3 *db)
{
    return db_executar_sql(db, "ROLLBACK;");
}

Resultado db_traduzir_erro(sqlite3 *db)
{
    int codigo;

    if (db == NULL) {
        return RES_ERRO_BANCO;
    }

    codigo = sqlite3_extended_errcode(db);
    switch (codigo) {
        case SQLITE_CONSTRAINT_UNIQUE:
        case SQLITE_CONSTRAINT_PRIMARYKEY:
            return RES_DUPLICADO;
        case SQLITE_CONSTRAINT_FOREIGNKEY:
        case SQLITE_CONSTRAINT_CHECK:
        case SQLITE_CONSTRAINT_NOTNULL:
            return RES_INVALIDO;
        default:
            return RES_ERRO_BANCO;
    }
}

void db_copiar_coluna(char *destino, size_t tamanho,
                      sqlite3_stmt *consulta, int coluna)
{
    const unsigned char *texto;

    if (destino == NULL || tamanho == 0 || consulta == NULL) {
        return;
    }
    texto = sqlite3_column_text(consulta, coluna);
    snprintf(destino, tamanho, "%s", texto != NULL ? (const char *)texto : "");
}

Resultado db_finalizar(sqlite3 *db, sqlite3_stmt *consulta,
                       Resultado resultado)
{
    int codigo = sqlite3_finalize(consulta);

    if (resultado == RES_OK && codigo != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    return resultado;
}

int db_vincular_texto(sqlite3_stmt *consulta, int indice, const char *texto,
                      int opcional)
{
    if (opcional && (texto == NULL || texto[0] == '\0')) {
        return sqlite3_bind_null(consulta, indice);
    }
    return sqlite3_bind_text(consulta, indice, texto, -1, SQLITE_TRANSIENT);
}
