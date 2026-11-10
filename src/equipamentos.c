/*
 * Implementa validacao, cadastro e consultas de equipamentos.
 * Tambem controla as telas do menu de equipamentos.
 */
#include "equipamentos.h"

#include "clientes.h"
#include "util.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TipoEquipamento {
    const char *codigo;
    const char *rotulo;
} TipoEquipamento;

static const TipoEquipamento equipamento_tipos[] = {
    {"COMPUTADOR", "Computador"},
    {"NOTEBOOK", "Notebook"},
    {"IMPRESSORA", "Impressora"},
    {"OUTRO", "Outro"}
};

static const char equipamento_consulta_colunas[] =
    "SELECT e.id, e.cliente_id, c.nome, e.tipo, e.marca, e.modelo, "
    "e.numero_serie FROM equipamentos e "
    "JOIN clientes c ON c.id = e.cliente_id ";
static const char equipamento_sql_todos[] =
    "SELECT e.id, e.cliente_id, c.nome, e.tipo, e.marca, e.modelo, "
    "e.numero_serie FROM equipamentos e "
    "JOIN clientes c ON c.id = e.cliente_id ORDER BY e.id;";
static const char equipamento_sql_cliente[] =
    "SELECT e.id, e.cliente_id, c.nome, e.tipo, e.marca, e.modelo, "
    "e.numero_serie FROM equipamentos e "
    "JOIN clientes c ON c.id = e.cliente_id "
    "WHERE e.cliente_id = ?1 ORDER BY e.id;";

static void equipamento_ler_colunas(sqlite3_stmt *consulta,
                                    Equipamento *equipamento)
{
    equipamento->id = sqlite3_column_int(consulta, 0);
    equipamento->cliente_id = sqlite3_column_int(consulta, 1);
    db_copiar_coluna(equipamento->cliente_nome,
                     sizeof(equipamento->cliente_nome), consulta, 2);
    db_copiar_coluna(equipamento->tipo, sizeof(equipamento->tipo), consulta,
                     3);
    db_copiar_coluna(equipamento->marca, sizeof(equipamento->marca), consulta,
                     4);
    db_copiar_coluna(equipamento->modelo, sizeof(equipamento->modelo), consulta,
                     5);
    db_copiar_coluna(equipamento->numero_serie,
                     sizeof(equipamento->numero_serie), consulta, 6);
}

static int equipamento_executar_insert(sqlite3_stmt *consulta,
                                       const Equipamento *equipamento)
{
    char numero_serie[sizeof(equipamento->numero_serie)];
    size_t i;
    int codigo;

    snprintf(numero_serie, sizeof(numero_serie), "%s",
             equipamento->numero_serie);
    for (i = 0; numero_serie[i] != '\0'; i++) {
        numero_serie[i] = (char)toupper((unsigned char)numero_serie[i]);
    }

    codigo = sqlite3_bind_int(consulta, 1, equipamento->cliente_id);
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 2, equipamento->tipo, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 3, equipamento->marca, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 4, equipamento->modelo, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 5, numero_serie, 0);
    }
    return codigo == SQLITE_OK ? sqlite3_step(consulta) : codigo;
}

static int equipamento_tipo_valido(const char *codigo)
{
    size_t i;

    if (codigo == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(equipamento_tipos) / sizeof(equipamento_tipos[0]);
         i++) {
        if (strcmp(codigo, equipamento_tipos[i].codigo) == 0) {
            return 1;
        }
    }
    return 0;
}

const char *equipamento_tipo_rotulo(const char *codigo)
{
    size_t i;

    if (codigo == NULL) {
        return "Desconhecido";
    }
    for (i = 0; i < sizeof(equipamento_tipos) / sizeof(equipamento_tipos[0]);
         i++) {
        if (strcmp(codigo, equipamento_tipos[i].codigo) == 0) {
            return equipamento_tipos[i].rotulo;
        }
    }
    return "Desconhecido";
}

const char *equipamento_tipo_codigo(int opcao)
{
    size_t quantidade = sizeof(equipamento_tipos) /
                        sizeof(equipamento_tipos[0]);

    if (opcao < 1 || (size_t)opcao > quantidade) {
        return NULL;
    }
    return equipamento_tipos[opcao - 1].codigo;
}

Resultado equipamento_validar(const Equipamento *equipamento)
{
    if (equipamento == NULL || equipamento->cliente_id <= 0 ||
        !equipamento_tipo_valido(equipamento->tipo) ||
        equipamento->marca[0] == '\0' || equipamento->modelo[0] == '\0' ||
        equipamento->numero_serie[0] == '\0') {
        return RES_INVALIDO;
    }
    return RES_OK;
}

Resultado equipamento_inserir(sqlite3 *db, const Equipamento *equipamento,
                              int *id_novo)
{
    static const char sql[] =
        "INSERT INTO equipamentos "
        "(cliente_id, tipo, marca, modelo, numero_serie) "
        "VALUES (?1, ?2, ?3, ?4, ?5);";
    Cliente cliente;
    sqlite3_stmt *consulta = NULL;
    int codigo;
    Resultado resultado = equipamento_validar(equipamento);

    if (db == NULL || id_novo == NULL || resultado != RES_OK) {
        return resultado != RES_OK ? resultado : RES_INVALIDO;
    }
    resultado = cliente_buscar_por_id(db, equipamento->cliente_id, &cliente);
    if (resultado != RES_OK) {
        return resultado;
    }
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    codigo = equipamento_executar_insert(consulta, equipamento);
    if (codigo != SQLITE_DONE) {
        resultado = db_traduzir_erro(db);
        return db_finalizar(db, consulta, resultado);
    }

    *id_novo = (int)sqlite3_last_insert_rowid(db);
    return db_finalizar(db, consulta, RES_OK);
}

Resultado equipamento_buscar_por_id(sqlite3 *db, int id,
                                    Equipamento *saida)
{
    char sql[sizeof(equipamento_consulta_colunas) + 32];
    sqlite3_stmt *consulta = NULL;
    int codigo;

    if (db == NULL || saida == NULL || id <= 0) {
        return RES_INVALIDO;
    }
    snprintf(sql, sizeof(sql), "%sWHERE e.id = ?1;", equipamento_consulta_colunas);
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }

    codigo = sqlite3_bind_int(consulta, 1, id);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    if (codigo == SQLITE_ROW) {
        equipamento_ler_colunas(consulta, saida);
        return db_finalizar(db, consulta, RES_OK);
    }
    if (codigo == SQLITE_DONE) {
        return db_finalizar(db, consulta, RES_NAO_ENCONTRADO);
    }
    return db_finalizar(db, consulta, db_traduzir_erro(db));
}

static Resultado equipamento_preparar_listagem(sqlite3 *db,
                                               sqlite3_stmt **consulta,
                                               int filtrar_cliente,
                                               int cliente_id)
{
    const char *sql = filtrar_cliente
        ? equipamento_sql_cliente : equipamento_sql_todos;

    if (sqlite3_prepare_v2(db, sql, -1, consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    if (filtrar_cliente &&
        sqlite3_bind_int(*consulta, 1, cliente_id) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    return RES_OK;
}

static Resultado equipamento_adicionar_linha(sqlite3_stmt *consulta,
                                             Equipamento **equipamentos,
                                             size_t *capacidade, int total)
{
    if ((size_t)total == *capacidade) {
        Equipamento *temporario = util_dobrar_capacidade(
            *equipamentos, capacidade, sizeof(**equipamentos));
        if (temporario == NULL) {
            return RES_ERRO_MEMORIA;
        }
        *equipamentos = temporario;
    }
    equipamento_ler_colunas(consulta, &(*equipamentos)[total]);
    return RES_OK;
}

static Resultado equipamento_coletar_linhas(sqlite3 *db,
                                            sqlite3_stmt *consulta,
                                            Equipamento **equipamentos,
                                            int *total)
{
    size_t capacidade = 8;
    int codigo;
    Resultado resultado = RES_OK;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        resultado = equipamento_adicionar_linha(consulta, equipamentos,
                                                &capacidade, *total);
        if (resultado != RES_OK) {
            return resultado;
        }
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado equipamento_publicar_lista(Equipamento *equipamentos,
                                            int total, Equipamento **lista,
                                            int *quantidade)
{
    if (total == 0) {
        free(equipamentos);
        return RES_OK;
    }
    *lista = equipamentos;
    *quantidade = total;
    return RES_OK;
}

static Resultado equipamento_listar_interno(sqlite3 *db, int filtrar_cliente,
                                            int cliente_id,
                                            Equipamento **lista,
                                            int *quantidade)
{
    sqlite3_stmt *consulta = NULL;
    Equipamento *equipamentos;
    int total = 0;
    Resultado resultado;

    if (db == NULL || lista == NULL || quantidade == NULL ||
        (filtrar_cliente && cliente_id <= 0)) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;
    equipamentos = malloc(8 * sizeof(*equipamentos));
    if (equipamentos == NULL) {
        return RES_ERRO_MEMORIA;
    }
    resultado = equipamento_preparar_listagem(db, &consulta, filtrar_cliente,
                                               cliente_id);
    if (resultado == RES_OK) {
        resultado = equipamento_coletar_linhas(db, consulta, &equipamentos,
                                               &total);
    }
    if (consulta != NULL) {
        resultado = db_finalizar(db, consulta, resultado);
    }
    if (resultado != RES_OK) {
        free(equipamentos);
        return resultado;
    }
    return equipamento_publicar_lista(equipamentos, total, lista, quantidade);
}

Resultado equipamento_listar(sqlite3 *db, Equipamento **lista,
                             int *quantidade)
{
    return equipamento_listar_interno(db, 0, 0, lista, quantidade);
}

Resultado equipamento_listar_por_cliente(sqlite3 *db, int cliente_id,
                                         Equipamento **lista,
                                         int *quantidade)
{
    return equipamento_listar_interno(db, 1, cliente_id, lista, quantidade);
}

static int equipamento_ler_id(const char *mensagem, int *id)
{
    Resultado resultado = util_ler_inteiro(mensagem, 1, INT_MAX, id);

    if (resultado == RES_ERRO_ARQUIVO) {
        return 0;
    }
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    return 1;
}

static int equipamento_ler_tipo(char *tipo, size_t tamanho)
{
    int opcao;
    Resultado resultado;
    const char *codigo;
    size_t quantidade = sizeof(equipamento_tipos) /
                        sizeof(equipamento_tipos[0]);
    size_t i;

    puts("Tipo do equipamento:");
    for (i = 0; i < quantidade; i++) {
        printf("%zu - %s\n", i + 1, equipamento_tipos[i].rotulo);
    }
    resultado = util_ler_inteiro("Escolha o tipo: ", 1, (int)quantidade,
                                 &opcao);
    if (resultado != RES_OK) {
        return 0;
    }
    codigo = equipamento_tipo_codigo(opcao);
    snprintf(tipo, tamanho, "%s", codigo);
    return 1;
}

static int equipamento_ler_dados(Equipamento *equipamento)
{
    Resultado resultado;

    if (!equipamento_ler_tipo(equipamento->tipo, sizeof(equipamento->tipo))) {
        return 0;
    }
    resultado = util_ler_texto_obrigatorio("Marca: ", equipamento->marca,
                                           sizeof(equipamento->marca));
    if (resultado == RES_OK) {
        resultado = util_ler_texto_obrigatorio("Modelo: ", equipamento->modelo,
                                               sizeof(equipamento->modelo));
    }
    if (resultado == RES_OK) {
        resultado = util_ler_texto_obrigatorio(
            "Número de série: ", equipamento->numero_serie,
            sizeof(equipamento->numero_serie));
    }
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    return 1;
}

static void equipamento_cadastrar(sqlite3 *db)
{
    Cliente cliente;
    Equipamento equipamento = {0};
    int id_novo;
    Resultado resultado;

    if (!equipamento_ler_id("ID do cliente: ", &equipamento.cliente_id)) {
        return;
    }
    resultado = cliente_buscar_por_id(db, equipamento.cliente_id, &cliente);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    printf("Cliente: %s\n", cliente.nome);
    if (!equipamento_ler_dados(&equipamento)) {
        return;
    }
    resultado = equipamento_inserir(db, &equipamento, &id_novo);
    if (resultado == RES_OK) {
        printf("[OK] Equipamento cadastrado com ID %d.\n", id_novo);
    } else {
        util_erro(util_texto_resultado(resultado));
    }
}

static void equipamento_mostrar_lista(sqlite3 *db, int filtrar_cliente,
                                      int cliente_id)
{
    Equipamento *lista = NULL;
    int quantidade = 0;
    int i;
    char id[16];
    Resultado resultado = filtrar_cliente
        ? equipamento_listar_por_cliente(db, cliente_id, &lista, &quantidade)
        : equipamento_listar(db, &lista, &quantidade);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhum equipamento cadastrado.");
    } else {
        util_imprimir_coluna("ID", 5);
        util_imprimir_coluna("Cliente", 22);
        util_imprimir_coluna("Tipo", 13);
        util_imprimir_coluna("Marca", 14);
        util_imprimir_coluna("Modelo", 14);
        util_imprimir_coluna("Nº de série", 18);
        putchar('\n');
        for (i = 0; i < quantidade; i++) {
            snprintf(id, sizeof(id), "%d", lista[i].id);
            util_imprimir_coluna(id, 5);
            util_imprimir_coluna(lista[i].cliente_nome, 22);
            util_imprimir_coluna(equipamento_tipo_rotulo(lista[i].tipo), 13);
            util_imprimir_coluna(lista[i].marca, 14);
            util_imprimir_coluna(lista[i].modelo, 14);
            util_imprimir_coluna(lista[i].numero_serie, 18);
            putchar('\n');
        }
    }
    free(lista);
}

static void equipamento_listar_cliente_tela(sqlite3 *db)
{
    int cliente_id;

    if (equipamento_ler_id("ID do cliente: ", &cliente_id)) {
        equipamento_mostrar_lista(db, 1, cliente_id);
    }
}

static void equipamento_consultar(sqlite3 *db)
{
    Equipamento equipamento;
    int id;
    Resultado resultado;

    if (!equipamento_ler_id("ID do equipamento: ", &id)) {
        return;
    }
    resultado = equipamento_buscar_por_id(db, id, &equipamento);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    printf("ID: %d\nCliente ID: %d\nCliente: %s\n", equipamento.id,
           equipamento.cliente_id, equipamento.cliente_nome);
    printf("Tipo: %s\nMarca: %s\nModelo: %s\nNúmero de série: %s\n",
           equipamento_tipo_rotulo(equipamento.tipo), equipamento.marca,
           equipamento.modelo, equipamento.numero_serie);
}

void menu_equipamentos(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("EQUIPAMENTOS");
        puts("1 - Cadastrar\n2 - Listar todos\n3 - Listar por cliente");
        puts("4 - Consultar por ID\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 4, &opcao);
        if (resultado != RES_OK) {
            return;
        }
        switch (opcao) {
            case 0:
                return;
            case 1:
                equipamento_cadastrar(db);
                break;
            case 2:
                equipamento_mostrar_lista(db, 0, 0);
                break;
            case 3:
                equipamento_listar_cliente_tela(db);
                break;
            case 4:
                equipamento_consultar(db);
                break;
            default:
                break;
        }
    }
}
