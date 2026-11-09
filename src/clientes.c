#include "clientes.h"

#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cliente_vincular_campos(sqlite3_stmt *consulta, const Cliente *c)
{
    int codigo = db_vincular_texto(consulta, 1, c->nome, 0);

    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 2, c->documento, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 3, c->telefone, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 4, c->email, 1);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 5, c->endereco, 1);
    }
    return codigo;
}

static void cliente_ler_colunas(sqlite3_stmt *consulta, Cliente *cliente)
{
    cliente->id = sqlite3_column_int(consulta, 0);
    db_copiar_coluna(cliente->nome, sizeof(cliente->nome), consulta, 1);
    db_copiar_coluna(cliente->documento, sizeof(cliente->documento), consulta,
                     2);
    db_copiar_coluna(cliente->telefone, sizeof(cliente->telefone), consulta, 3);
    db_copiar_coluna(cliente->email, sizeof(cliente->email), consulta, 4);
    db_copiar_coluna(cliente->endereco, sizeof(cliente->endereco), consulta, 5);
    db_copiar_coluna(cliente->data_cadastro, sizeof(cliente->data_cadastro),
                     consulta, 6);
}

Resultado cliente_validar(const Cliente *cliente)
{
    if (cliente == NULL || cliente->nome[0] == '\0' ||
        !util_validar_documento(cliente->documento) ||
        !util_validar_telefone(cliente->telefone) ||
        (cliente->email[0] != '\0' &&
         !util_validar_email(cliente->email))) {
        return RES_INVALIDO;
    }
    return RES_OK;
}

Resultado cliente_inserir(sqlite3 *db, const Cliente *cliente, int *id_novo)
{
    static const char sql[] =
        "INSERT INTO clientes (nome, documento, telefone, email, endereco) "
        "VALUES (?1, ?2, ?3, ?4, ?5);";
    sqlite3_stmt *consulta = NULL;
    int codigo;
    Resultado resultado = cliente_validar(cliente);

    if (db == NULL || id_novo == NULL || resultado != RES_OK) {
        return resultado != RES_OK ? resultado : RES_INVALIDO;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }

    codigo = cliente_vincular_campos(consulta, cliente);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }

    if (codigo != SQLITE_DONE) {
        resultado = db_traduzir_erro(db);
        return db_finalizar(db, consulta, resultado);
    }

    *id_novo = (int)sqlite3_last_insert_rowid(db);
    return db_finalizar(db, consulta, RES_OK);
}

Resultado cliente_buscar_por_id(sqlite3 *db, int id, Cliente *saida)
{
    static const char sql[] =
        "SELECT id, nome, documento, telefone, email, endereco, data_cadastro "
        "FROM clientes WHERE id = ?1;";
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
        cliente_ler_colunas(consulta, saida);
        return db_finalizar(db, consulta, RES_OK);
    }

    if (codigo == SQLITE_DONE) {
        return db_finalizar(db, consulta, RES_NAO_ENCONTRADO);
    }
    return db_finalizar(db, consulta, db_traduzir_erro(db));
}

Resultado cliente_atualizar(sqlite3 *db, const Cliente *cliente)
{
    static const char sql[] =
        "UPDATE clientes SET nome = ?1, documento = ?2, telefone = ?3, "
        "email = ?4, endereco = ?5 WHERE id = ?6;";
    sqlite3_stmt *consulta = NULL;
    int codigo;
    Resultado resultado = cliente_validar(cliente);

    if (db == NULL || cliente == NULL || cliente->id <= 0 ||
        resultado != RES_OK) {
        return resultado != RES_OK ? resultado : RES_INVALIDO;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }

    codigo = cliente_vincular_campos(consulta, cliente);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_bind_int(consulta, 6, cliente->id);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }

    if (codigo != SQLITE_DONE) {
        resultado = db_traduzir_erro(db);
    } else if (sqlite3_changes(db) == 0) {
        resultado = RES_NAO_ENCONTRADO;
    } else {
        resultado = RES_OK;
    }
    return db_finalizar(db, consulta, resultado);
}

static Resultado cliente_carregar_linhas(sqlite3 *db, sqlite3_stmt *consulta,
                                         Cliente **clientes,
                                         size_t *capacidade, int *total)
{
    int codigo;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        if (*total == INT_MAX) {
            return RES_ERRO_MEMORIA;
        }
        if ((size_t)*total == *capacidade) {
            Cliente *temporario = util_dobrar_capacidade(
                *clientes, capacidade, sizeof(**clientes));
            if (temporario == NULL) {
                return RES_ERRO_MEMORIA;
            }
            *clientes = temporario;
        }
        cliente_ler_colunas(consulta, &(*clientes)[*total]);
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado cliente_publicar_lista(Cliente *clientes, int total,
                                        Cliente **lista, int *quantidade)
{
    if (total == 0) {
        free(clientes);
        return RES_OK;
    }
    *lista = clientes;
    *quantidade = total;
    return RES_OK;
}

Resultado cliente_listar(sqlite3 *db, Cliente **lista, int *quantidade)
{
    static const char sql[] =
        "SELECT id, nome, documento, telefone, email, endereco, data_cadastro "
        "FROM clientes ORDER BY id;";
    sqlite3_stmt *consulta = NULL;
    Cliente *clientes;
    size_t capacidade = 8;
    int total = 0;
    Resultado resultado;

    if (db == NULL || lista == NULL || quantidade == NULL) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;

    clientes = malloc(capacidade * sizeof(*clientes));
    if (clientes == NULL) {
        return RES_ERRO_MEMORIA;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        free(clientes);
        return db_traduzir_erro(db);
    }

    resultado = cliente_carregar_linhas(db, consulta, &clientes, &capacidade,
                                        &total);
    resultado = db_finalizar(db, consulta, resultado);
    if (resultado != RES_OK) {
        free(clientes);
        return resultado;
    }
    return cliente_publicar_lista(clientes, total, lista, quantidade);
}

static void cliente_mostrar_dados(const Cliente *cliente)
{
    printf("ID: %d\n", cliente->id);
    printf("Nome: %s\n", cliente->nome);
    printf("Documento: %s\n", cliente->documento);
    printf("Telefone: %s\n", cliente->telefone);
    printf("Email: %s\n", cliente->email[0] != '\0' ? cliente->email : "-");
    printf("Endereço: %s\n",
           cliente->endereco[0] != '\0' ? cliente->endereco : "-");
    printf("Data de cadastro: %s\n", cliente->data_cadastro);
}

static Resultado cliente_ler_nome(Cliente *cliente, int manter)
{
    char texto[sizeof(cliente->nome)];
    Resultado resultado;

    if (manter) {
        resultado = util_ler_linha("Nome (Enter mantém): ", texto,
                                   sizeof(texto));
        if (resultado != RES_OK) {
            return resultado;
        }
        if (texto[0] != '\0') {
            snprintf(cliente->nome, sizeof(cliente->nome), "%s", texto);
        }
        return RES_OK;
    }
    return util_ler_texto_obrigatorio("Nome: ", cliente->nome,
                                      sizeof(cliente->nome));
}

static Resultado cliente_ler_documento(Cliente *cliente, int manter)
{
    char texto[64];
    char digitos[sizeof(texto)];
    Resultado resultado;

    for (;;) {
        resultado = util_ler_linha(
            manter ? "Documento (Enter mantém): " : "Documento: ",
            texto, sizeof(texto));
        if (resultado != RES_OK) {
            return resultado;
        }
        if (manter && texto[0] == '\0') {
            return RES_OK;
        }
        util_somente_digitos(texto, digitos, sizeof(digitos));
        if (util_validar_documento(digitos)) {
            snprintf(cliente->documento, sizeof(cliente->documento), "%.*s",
                     (int)sizeof(cliente->documento) - 1, digitos);
            return RES_OK;
        }
        util_erro("Documento inválido. Informe 11 ou 14 dígitos.");
    }
}

static Resultado cliente_ler_telefone(Cliente *cliente, int manter)
{
    char texto[64];
    char digitos[sizeof(texto)];
    Resultado resultado;

    for (;;) {
        resultado = util_ler_linha(
            manter ? "Telefone (Enter mantém): " : "Telefone: ",
            texto, sizeof(texto));
        if (resultado != RES_OK) {
            return resultado;
        }
        if (manter && texto[0] == '\0') {
            return RES_OK;
        }
        util_somente_digitos(texto, digitos, sizeof(digitos));
        if (util_validar_telefone(digitos)) {
            snprintf(cliente->telefone, sizeof(cliente->telefone), "%.*s",
                     (int)sizeof(cliente->telefone) - 1, digitos);
            return RES_OK;
        }
        util_erro("Telefone inválido. Informe 10 ou 11 dígitos.");
    }
}

static Resultado cliente_ler_email(Cliente *cliente, int manter)
{
    char texto[sizeof(cliente->email)];
    Resultado resultado;

    for (;;) {
        resultado = util_ler_linha(
            manter ? "Email (Enter mantém; - para vazio): "
                   : "Email (opcional, Enter para pular): ",
            texto, sizeof(texto));
        if (resultado != RES_OK) {
            return resultado;
        }
        if (manter && texto[0] == '\0') {
            return RES_OK;
        }
        if (strcmp(texto, "-") == 0) {
            cliente->email[0] = '\0';
            return RES_OK;
        }
        if (texto[0] == '\0' || util_validar_email(texto)) {
            snprintf(cliente->email, sizeof(cliente->email), "%s", texto);
            return RES_OK;
        }
        util_erro("Email inválido.");
    }
}

static Resultado cliente_ler_endereco(Cliente *cliente, int manter)
{
    char texto[sizeof(cliente->endereco)];
    Resultado resultado = util_ler_linha(
        manter ? "Endereço (Enter mantém; - para vazio): "
               : "Endereço (opcional, Enter para pular): ",
        texto, sizeof(texto));

    if (resultado != RES_OK) {
        return resultado;
    }
    if (texto[0] == '\0' && manter) {
        return RES_OK;
    }
    if (strcmp(texto, "-") == 0) {
        cliente->endereco[0] = '\0';
    } else if (texto[0] != '\0' || !manter) {
        snprintf(cliente->endereco, sizeof(cliente->endereco), "%s", texto);
    }
    return RES_OK;
}

static int cliente_ler_id(int *id)
{
    Resultado resultado = util_ler_inteiro("ID do cliente: ", 1, INT_MAX, id);

    if (resultado == RES_ERRO_ARQUIVO) {
        return 0;
    }
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    return 1;
}

static void cliente_cadastrar(sqlite3 *db)
{
    Cliente cliente = {0};
    int id_novo;
    Resultado resultado = cliente_ler_nome(&cliente, 0);

    if (resultado == RES_OK) {
        resultado = cliente_ler_documento(&cliente, 0);
    }
    if (resultado == RES_OK) {
        resultado = cliente_ler_telefone(&cliente, 0);
    }
    if (resultado == RES_OK) {
        resultado = cliente_ler_email(&cliente, 0);
    }
    if (resultado == RES_OK) {
        resultado = cliente_ler_endereco(&cliente, 0);
    }
    if (resultado == RES_OK) {
        resultado = cliente_inserir(db, &cliente, &id_novo);
    }
    if (resultado == RES_OK) {
        printf("[OK] Cliente cadastrado com ID %d.\n", id_novo);
    } else {
        util_erro(util_texto_resultado(resultado));
    }
}

static void cliente_listar_tela(sqlite3 *db)
{
    Cliente *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = cliente_listar(db, &lista, &quantidade);
    char id[16];

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhum cliente cadastrado.");
    } else {
        util_imprimir_coluna("ID", 6);
        util_imprimir_coluna("Nome", 30);
        util_imprimir_coluna("Documento", 14);
        util_imprimir_coluna("Telefone", 11);
        putchar('\n');
        for (i = 0; i < quantidade; i++) {
            snprintf(id, sizeof(id), "%d", lista[i].id);
            util_imprimir_coluna(id, 6);
            util_imprimir_coluna(lista[i].nome, 30);
            util_imprimir_coluna(lista[i].documento, 14);
            util_imprimir_coluna(lista[i].telefone, 11);
            putchar('\n');
        }
    }
    free(lista);
}

static void cliente_consultar(sqlite3 *db)
{
    Cliente cliente;
    int id;
    Resultado resultado;

    if (!cliente_ler_id(&id)) {
        return;
    }
    resultado = cliente_buscar_por_id(db, id, &cliente);
    if (resultado == RES_OK) {
        cliente_mostrar_dados(&cliente);
    } else {
        util_erro(util_texto_resultado(resultado));
    }
}

static Resultado cliente_ler_alteracoes(Cliente *cliente)
{
    Resultado resultado = cliente_ler_nome(cliente, 1);

    if (resultado == RES_OK) {
        resultado = cliente_ler_documento(cliente, 1);
    }
    if (resultado == RES_OK) {
        resultado = cliente_ler_telefone(cliente, 1);
    }
    if (resultado == RES_OK) {
        resultado = cliente_ler_email(cliente, 1);
    }
    if (resultado == RES_OK) {
        resultado = cliente_ler_endereco(cliente, 1);
    }
    return resultado == RES_OK ? cliente_validar(cliente) : resultado;
}

static void cliente_atualizar_tela(sqlite3 *db)
{
    Cliente cliente;
    int id;
    int confirmado;
    Resultado resultado;

    if (!cliente_ler_id(&id)) {
        return;
    }
    resultado = cliente_buscar_por_id(db, id, &cliente);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    cliente_mostrar_dados(&cliente);
    resultado = cliente_ler_alteracoes(&cliente);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    resultado = util_ler_confirmacao("Salvar alterações? (S/N): ",
                                     &confirmado);
    if (resultado != RES_OK || !confirmado) {
        return;
    }
    resultado = cliente_atualizar(db, &cliente);
    if (resultado == RES_OK) {
        util_sucesso("Cliente atualizado.");
    } else {
        util_erro(util_texto_resultado(resultado));
    }
}

void menu_clientes(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("CLIENTES");
        puts("1 - Cadastrar\n2 - Listar\n3 - Consultar por ID");
        puts("4 - Atualizar\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 4, &opcao);
        if (resultado != RES_OK) {
            return;
        }

        switch (opcao) {
            case 0:
                return;
            case 1:
                cliente_cadastrar(db);
                break;
            case 2:
                cliente_listar_tela(db);
                break;
            case 3:
                cliente_consultar(db);
                break;
            case 4:
                cliente_atualizar_tela(db);
                break;
            default:
                break;
        }
    }
}
