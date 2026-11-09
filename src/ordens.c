#include "ordens.h"

#include "clientes.h"
#include "equipamentos.h"
#include "historico.h"
#include "tecnicos.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const os_codigos[OS_STATUS_TOTAL] = {
    "ABERTA", "EM_DIAGNOSTICO", "AGUARDANDO_APROVACAO", "APROVADA",
    "AGUARDANDO_PECA", "EM_REPARO", "CONCLUIDA", "ENTREGUE", "CANCELADA"
};
static const char *const os_rotulos[OS_STATUS_TOTAL] = {
    "Aberta", "Em diagnóstico", "Aguardando aprovação", "Aprovada",
    "Aguardando peça", "Em reparo", "Concluída", "Entregue", "Cancelada"
};
static const char os_sql_colunas[] =
    "SELECT o.id, o.equipamento_id, c.id, c.nome, e.tipo, e.marca, "
    "e.modelo, e.numero_serie, COALESCE(t.id, 0), COALESCE(t.nome, ''), "
    "o.descricao_problema, o.diagnostico, o.status, o.data_abertura "
    "FROM ordens_servico o JOIN equipamentos e ON e.id = o.equipamento_id "
    "JOIN clientes c ON c.id = e.cliente_id "
    "LEFT JOIN tecnicos t ON t.id = o.tecnico_id ";
static const char os_sql_todas[] =
    "SELECT o.id, o.equipamento_id, c.id, c.nome, e.tipo, e.marca, "
    "e.modelo, e.numero_serie, COALESCE(t.id, 0), COALESCE(t.nome, ''), "
    "o.descricao_problema, o.diagnostico, o.status, o.data_abertura "
    "FROM ordens_servico o JOIN equipamentos e ON e.id = o.equipamento_id "
    "JOIN clientes c ON c.id = e.cliente_id "
    "LEFT JOIN tecnicos t ON t.id = o.tecnico_id ORDER BY o.id;";
static const char os_sql_status[] =
    "SELECT o.id, o.equipamento_id, c.id, c.nome, e.tipo, e.marca, "
    "e.modelo, e.numero_serie, COALESCE(t.id, 0), COALESCE(t.nome, ''), "
    "o.descricao_problema, o.diagnostico, o.status, o.data_abertura "
    "FROM ordens_servico o JOIN equipamentos e ON e.id = o.equipamento_id "
    "JOIN clientes c ON c.id = e.cliente_id "
    "LEFT JOIN tecnicos t ON t.id = o.tecnico_id "
    "WHERE o.status = ?1 ORDER BY o.id;";

const char *os_status_codigo(StatusOS status)
{
    if (status < OS_ABERTA || status >= OS_STATUS_TOTAL) {
        return NULL;
    }
    return os_codigos[status];
}

const char *os_status_rotulo(StatusOS status)
{
    if (status < OS_ABERTA || status >= OS_STATUS_TOTAL) {
        return NULL;
    }
    return os_rotulos[status];
}

int os_status_de_codigo(const char *codigo)
{
    int i;

    if (codigo == NULL) {
        return -1;
    }
    for (i = 0; i < OS_STATUS_TOTAL; i++) {
        if (strcmp(codigo, os_codigos[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static void ordem_ler_colunas(sqlite3_stmt *consulta, OrdemServico *ordem)
{
    ordem->id = sqlite3_column_int(consulta, 0);
    ordem->equipamento_id = sqlite3_column_int(consulta, 1);
    ordem->cliente_id = sqlite3_column_int(consulta, 2);
    db_copiar_coluna(ordem->cliente_nome, sizeof(ordem->cliente_nome),
                     consulta, 3);
    db_copiar_coluna(ordem->equip_tipo, sizeof(ordem->equip_tipo), consulta, 4);
    db_copiar_coluna(ordem->equip_marca, sizeof(ordem->equip_marca),
                     consulta, 5);
    db_copiar_coluna(ordem->equip_modelo, sizeof(ordem->equip_modelo),
                     consulta, 6);
    db_copiar_coluna(ordem->equip_serie, sizeof(ordem->equip_serie),
                     consulta, 7);
    ordem->tecnico_id = sqlite3_column_int(consulta, 8);
    db_copiar_coluna(ordem->tecnico_nome, sizeof(ordem->tecnico_nome),
                     consulta, 9);
    db_copiar_coluna(ordem->descricao_problema,
                     sizeof(ordem->descricao_problema), consulta, 10);
    db_copiar_coluna(ordem->diagnostico, sizeof(ordem->diagnostico), consulta,
                     11);
    db_copiar_coluna(ordem->status, sizeof(ordem->status), consulta, 12);
    db_copiar_coluna(ordem->data_abertura, sizeof(ordem->data_abertura),
                     consulta, 13);
}

static Resultado ordem_inserir(sqlite3 *db, int equipamento_id, int tecnico_id,
                               const char *descricao, int *id_novo)
{
    static const char sql[] =
        "INSERT INTO ordens_servico "
        "(equipamento_id, tecnico_id, descricao_problema, status) "
        "VALUES (?1, ?2, ?3, 'ABERTA');";
    sqlite3_stmt *consulta = NULL;
    int codigo;
    Resultado resultado;

    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    codigo = sqlite3_bind_int(consulta, 1, equipamento_id);
    if (codigo == SQLITE_OK) {
        codigo = tecnico_id == 0 ? sqlite3_bind_null(consulta, 2)
                                 : sqlite3_bind_int(consulta, 2, tecnico_id);
    }
    if (codigo == SQLITE_OK) {
        codigo = db_vincular_texto(consulta, 3, descricao, 0);
    }
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    resultado = codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
    if (resultado == RES_OK) {
        *id_novo = (int)sqlite3_last_insert_rowid(db);
    }
    return db_finalizar(db, consulta, resultado);
}

static Resultado ordem_registrar_abertura(sqlite3 *db, int ordem_id,
                                          int tecnico_id,
                                          const Tecnico *tecnico)
{
    char descricao[160];
    Resultado resultado = historico_registrar(db, ordem_id, "ABERTURA", NULL,
                                                "ABERTA", "OS aberta");

    if (resultado != RES_OK || tecnico_id == 0) {
        return resultado;
    }
    snprintf(descricao, sizeof(descricao), "Técnico atribuído: %s",
             tecnico->nome);
    return historico_registrar(db, ordem_id, "TECNICO", NULL, NULL, descricao);
}

static Resultado ordem_reverter(sqlite3 *db, Resultado erro)
{
    (void)db_desfazer(db);
    return erro;
}

Resultado ordem_abrir(sqlite3 *db, int equipamento_id, int tecnico_id,
                      const char *descricao_problema, int *id_novo)
{
    Equipamento equipamento;
    Tecnico tecnico;
    Resultado resultado;

    if (db == NULL || id_novo == NULL || equipamento_id <= 0 ||
        tecnico_id < 0 || descricao_problema == NULL ||
        descricao_problema[0] == '\0') {
        return RES_INVALIDO;
    }
    resultado = equipamento_buscar_por_id(db, equipamento_id, &equipamento);
    if (resultado != RES_OK) {
        return resultado;
    }
    if (tecnico_id > 0) {
        resultado = tecnico_buscar_por_id(db, tecnico_id, &tecnico);
        if (resultado != RES_OK) {
            return resultado;
        }
    }
    resultado = db_iniciar_transacao(db);
    if (resultado != RES_OK) {
        return resultado;
    }
    resultado = ordem_inserir(db, equipamento_id, tecnico_id,
                              descricao_problema, id_novo);
    if (resultado == RES_OK) {
        resultado = ordem_registrar_abertura(db, *id_novo, tecnico_id, &tecnico);
    }
    if (resultado != RES_OK) {
        return ordem_reverter(db, resultado);
    }
    resultado = db_confirmar(db);
    return resultado == RES_OK ? RES_OK : ordem_reverter(db, resultado);
}

Resultado ordem_buscar_por_id(sqlite3 *db, int id, OrdemServico *saida)
{
    char sql[sizeof(os_sql_colunas) + 32];
    sqlite3_stmt *consulta = NULL;
    int codigo;

    if (db == NULL || saida == NULL || id <= 0) {
        return RES_INVALIDO;
    }
    snprintf(sql, sizeof(sql), "%sWHERE o.id = ?1;", os_sql_colunas);
    if (sqlite3_prepare_v2(db, sql, -1, &consulta, NULL) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    codigo = sqlite3_bind_int(consulta, 1, id);
    if (codigo == SQLITE_OK) {
        codigo = sqlite3_step(consulta);
    }
    if (codigo == SQLITE_ROW) {
        ordem_ler_colunas(consulta, saida);
        return db_finalizar(db, consulta, RES_OK);
    }
    if (codigo == SQLITE_DONE) {
        return db_finalizar(db, consulta, RES_NAO_ENCONTRADO);
    }
    return db_finalizar(db, consulta, db_traduzir_erro(db));
}

static Resultado ordem_adicionar_linha(sqlite3_stmt *consulta,
                                      OrdemServico **ordens,
                                      size_t *capacidade, int quantidade)
{
    if ((size_t)quantidade == *capacidade) {
        OrdemServico *temporario = util_dobrar_capacidade(
            *ordens, capacidade, sizeof(**ordens));
        if (temporario == NULL) {
            return RES_ERRO_MEMORIA;
        }
        *ordens = temporario;
    }
    ordem_ler_colunas(consulta, &(*ordens)[quantidade]);
    return RES_OK;
}

static Resultado ordem_ler_linhas(sqlite3 *db, sqlite3_stmt *consulta,
                                  OrdemServico **ordens, size_t *capacidade,
                                  int *total)
{
    int codigo;

    *total = 0;
    while ((codigo = sqlite3_step(consulta)) == SQLITE_ROW) {
        if (*total == INT_MAX) {
            return RES_ERRO_MEMORIA;
        }
        {
            Resultado resultado = ordem_adicionar_linha(
                consulta, ordens, capacidade, *total);
            if (resultado != RES_OK) {
                return resultado;
            }
        }
        (*total)++;
    }
    return codigo == SQLITE_DONE ? RES_OK : db_traduzir_erro(db);
}

static Resultado ordem_listar_preparar(sqlite3 *db, const char *status_codigo,
                                       sqlite3_stmt **consulta)
{
    const char *sql = status_codigo == NULL ? os_sql_todas : os_sql_status;
    int codigo = sqlite3_prepare_v2(db, sql, -1, consulta, NULL);

    if (codigo != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    if (status_codigo != NULL &&
        db_vincular_texto(*consulta, 1, status_codigo, 0) != SQLITE_OK) {
        return db_traduzir_erro(db);
    }
    return RES_OK;
}

static Resultado ordem_publicar_lista(OrdemServico *ordens, int total,
                                      OrdemServico **lista, int *quantidade)
{
    if (total == 0) {
        free(ordens);
        return RES_OK;
    }
    *lista = ordens;
    *quantidade = total;
    return RES_OK;
}

static void ordem_imprimir_cabecalho(void);
static void ordem_imprimir_linha(const OrdemServico *ordem, char *id,
                                 size_t id_tamanho, char *data,
                                 size_t data_tamanho);

Resultado ordem_listar(sqlite3 *db, const char *status_codigo,
                       OrdemServico **lista, int *quantidade)
{
    sqlite3_stmt *consulta = NULL;
    OrdemServico *ordens;
    size_t capacidade = 8;
    int total = 0;
    Resultado resultado;

    if (db == NULL || lista == NULL || quantidade == NULL ||
        (status_codigo != NULL && os_status_de_codigo(status_codigo) < 0)) {
        return RES_INVALIDO;
    }
    *lista = NULL;
    *quantidade = 0;
    ordens = malloc(capacidade * sizeof(*ordens));
    if (ordens == NULL) {
        return RES_ERRO_MEMORIA;
    }
    resultado = ordem_listar_preparar(db, status_codigo, &consulta);
    if (resultado == RES_OK) {
        resultado = ordem_ler_linhas(db, consulta, &ordens, &capacidade, &total);
    }
    if (consulta != NULL) {
        resultado = db_finalizar(db, consulta, resultado);
    }
    if (resultado != RES_OK) {
        free(ordens);
        return resultado;
    }
    return ordem_publicar_lista(ordens, total, lista, quantidade);
}

static int ordem_ler_id(const char *mensagem, int *id, int permitir_zero)
{
    int minimo = permitir_zero ? 0 : 1;
    Resultado resultado = util_ler_inteiro(mensagem, minimo, INT_MAX, id);

    if (resultado == RES_ERRO_ARQUIVO) {
        return 0;
    }
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    return 1;
}

static int ordem_selecionar_equipamento(sqlite3 *db, int cliente_id,
                                        int *equipamento_id)
{
    Equipamento *lista = NULL;
    int quantidade = 0;
    Resultado resultado = equipamento_listar_por_cliente(
        db, cliente_id, &lista, &quantidade);
    int i;

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        free(lista);
        return 0;
    }
    if (quantidade == 0) {
        puts("Cadastre um equipamento para este cliente primeiro.");
        free(lista);
        return 0;
    }
    for (i = 0; i < quantidade; i++) {
        printf("ID %d - %s %s (%s)\n", lista[i].id, lista[i].marca,
               lista[i].modelo, equipamento_tipo_rotulo(lista[i].tipo));
    }
    free(lista);
    return ordem_ler_id("ID do equipamento: ", equipamento_id, 0);
}

static int ordem_confirmar_equipamento(sqlite3 *db, int cliente_id,
                                       int equipamento_id)
{
    Equipamento equipamento;
    Resultado resultado = equipamento_buscar_por_id(db, equipamento_id,
                                                     &equipamento);

    if (resultado != RES_OK || equipamento.cliente_id != cliente_id) {
        util_erro(resultado == RES_OK
                      ? "O equipamento não pertence ao cliente informado."
                      : util_texto_resultado(resultado));
        return 0;
    }
    return 1;
}

static void ordem_mostrar_tecnicos(sqlite3 *db)
{
    Tecnico *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = tecnico_listar(db, &lista, &quantidade);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhum técnico cadastrado. Digite 0 para continuar sem técnico.");
    } else {
        for (i = 0; i < quantidade; i++) {
            printf("ID %d - %s (%s)\n", lista[i].id, lista[i].nome,
                   lista[i].especialidade);
        }
    }
    free(lista);
}

static int ordem_ler_tecnico(sqlite3 *db, int *tecnico_id)
{
    Tecnico tecnico;

    ordem_mostrar_tecnicos(db);
    if (!ordem_ler_id("ID do técnico (0 = sem técnico): ", tecnico_id, 1)) {
        return 0;
    }
    if (*tecnico_id == 0) {
        return 1;
    }
    if (tecnico_buscar_por_id(db, *tecnico_id, &tecnico) != RES_OK) {
        util_erro("Técnico não encontrado.");
        return 0;
    }
    return 1;
}

static int ordem_ler_cliente(sqlite3 *db, int *cliente_id)
{
    Cliente cliente;
    Resultado resultado;

    if (!ordem_ler_id("ID do cliente: ", cliente_id, 0)) {
        return 0;
    }
    resultado = cliente_buscar_por_id(db, *cliente_id, &cliente);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    printf("Cliente: %s\n", cliente.nome);
    return 1;
}

static void ordem_abrir_tela(sqlite3 *db)
{
    char descricao[501];
    int cliente_id;
    int equipamento_id;
    int tecnico_id;
    int confirmado;
    int id_novo;
    Resultado resultado;

    if (!ordem_ler_cliente(db, &cliente_id) ||
        !ordem_selecionar_equipamento(db, cliente_id, &equipamento_id) ||
        !ordem_confirmar_equipamento(db, cliente_id, equipamento_id) ||
        !ordem_ler_tecnico(db, &tecnico_id)) {
        return;
    }
    resultado = util_ler_texto_obrigatorio("Descrição do problema: ",
                                            descricao, sizeof(descricao));
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    resultado = util_ler_confirmacao("Abrir ordem? (S/N): ", &confirmado);
    if (resultado != RES_OK || !confirmado) {
        return;
    }
    resultado = ordem_abrir(db, equipamento_id, tecnico_id, descricao,
                            &id_novo);
    if (resultado == RES_OK) {
        printf("[OK] Ordem de serviço %d aberta.\n", id_novo);
    } else {
        util_erro(util_texto_resultado(resultado));
    }
}

static void ordem_mostrar_dados(const OrdemServico *ordem)
{
    char data[20];
    int status = os_status_de_codigo(ordem->status);

    util_formatar_data(ordem->data_abertura, data, sizeof(data));
    printf("ID: %d\nCliente: %s (ID %d)\n", ordem->id, ordem->cliente_nome,
           ordem->cliente_id);
    printf("Equipamento: %s %s %s, série %s (ID %d)\n",
           equipamento_tipo_rotulo(ordem->equip_tipo), ordem->equip_marca,
           ordem->equip_modelo, ordem->equip_serie, ordem->equipamento_id);
    printf("Técnico: %s\n", ordem->tecnico_id == 0 ? "-" : ordem->tecnico_nome);
    printf("Descrição: %s\nDiagnóstico: %s\n", ordem->descricao_problema,
           ordem->diagnostico[0] == '\0' ? "-" : ordem->diagnostico);
    printf("Situação: %s\nAbertura: %s\n",
           status >= 0 ? os_status_rotulo((StatusOS)status) : ordem->status,
           data);
}

static void ordem_consultar_tela(sqlite3 *db)
{
    OrdemServico ordem;
    int id;
    Resultado resultado;

    if (!ordem_ler_id("ID da ordem: ", &id, 0)) {
        return;
    }
    resultado = ordem_buscar_por_id(db, id, &ordem);
    if (resultado == RES_OK) {
        ordem_mostrar_dados(&ordem);
    } else {
        util_erro(util_texto_resultado(resultado));
    }
}

static void ordem_listar_tela(sqlite3 *db, const char *status)
{
    OrdemServico *lista = NULL;
    int quantidade = 0;
    int i;
    char id[16];
    char data[20];
    Resultado resultado = ordem_listar(db, status, &lista, &quantidade);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhuma ordem encontrada.");
    } else {
        ordem_imprimir_cabecalho();
        for (i = 0; i < quantidade; i++) {
            ordem_imprimir_linha(&lista[i], id, sizeof(id), data, sizeof(data));
        }
    }
    free(lista);
}

static void ordem_imprimir_cabecalho(void)
{
    util_imprimir_coluna("ID", 5);
    util_imprimir_coluna("Cliente", 22);
    util_imprimir_coluna("Equipamento", 28);
    util_imprimir_coluna("Técnico", 20);
    util_imprimir_coluna("Situação", 24);
    util_imprimir_coluna("Abertura", 19);
    putchar('\n');
}

static void ordem_imprimir_linha(const OrdemServico *ordem, char *id,
                                 size_t id_tamanho, char *data,
                                 size_t data_tamanho)
{
    char equipamento[124];
    int status = os_status_de_codigo(ordem->status);

    snprintf(id, id_tamanho, "%d", ordem->id);
    snprintf(equipamento, sizeof(equipamento), "%s %s", ordem->equip_marca,
             ordem->equip_modelo);
    util_formatar_data(ordem->data_abertura, data, data_tamanho);
    util_imprimir_coluna(id, 5);
    util_imprimir_coluna(ordem->cliente_nome, 22);
    util_imprimir_coluna(equipamento, 28);
    util_imprimir_coluna(ordem->tecnico_id == 0 ? "-" : ordem->tecnico_nome,
                         20);
    util_imprimir_coluna(status >= 0
                             ? os_status_rotulo((StatusOS)status)
                             : ordem->status,
                         24);
    util_imprimir_coluna(data, 19);
    putchar('\n');
}

static int ordem_selecionar_status(void)
{
    int opcao;
    int i;
    Resultado resultado;

    for (i = 0; i < OS_STATUS_TOTAL; i++) {
        printf("%d - %s\n", i + 1, os_status_rotulo((StatusOS)i));
    }
    resultado = util_ler_inteiro("Escolha a situação: ", 1, OS_STATUS_TOTAL,
                                 &opcao);
    return resultado == RES_OK ? opcao - 1 : -1;
}

static void ordem_status_tela(sqlite3 *db)
{
    int status = ordem_selecionar_status();

    if (status >= 0) {
        ordem_listar_tela(db, os_status_codigo((StatusOS)status));
    }
}

static const char *ordem_tipo_evento_rotulo(const char *tipo)
{
    if (strcmp(tipo, "ABERTURA") == 0) return "Abertura";
    if (strcmp(tipo, "STATUS") == 0) return "Situação";
    if (strcmp(tipo, "DIAGNOSTICO") == 0) return "Diagnóstico";
    if (strcmp(tipo, "OBSERVACAO") == 0) return "Observação";
    if (strcmp(tipo, "TECNICO") == 0) return "Técnico";
    if (strcmp(tipo, "PECA") == 0) return "Peça";
    return tipo;
}

static void ordem_mostrar_evento(const EventoHistorico *evento)
{
    char data[20];
    int anterior = os_status_de_codigo(evento->status_anterior);
    int novo = os_status_de_codigo(evento->status_novo);

    util_formatar_data(evento->data_hora, data, sizeof(data));
    printf("%s - %s", data, ordem_tipo_evento_rotulo(evento->tipo_evento));
    if (evento->status_anterior[0] != '\0' && evento->status_novo[0] != '\0') {
        printf(": %s → %s",
               anterior >= 0 ? os_status_rotulo((StatusOS)anterior)
                             : evento->status_anterior,
               novo >= 0 ? os_status_rotulo((StatusOS)novo)
                         : evento->status_novo);
    }
    if (evento->descricao[0] != '\0') {
        printf(" — %s", evento->descricao);
    }
    putchar('\n');
}

static void ordem_historico_tela(sqlite3 *db)
{
    EventoHistorico *lista = NULL;
    int quantidade = 0;
    int id;
    int i;
    OrdemServico ordem;
    Resultado resultado;

    if (!ordem_ler_id("ID da ordem: ", &id, 0)) {
        return;
    }
    resultado = ordem_buscar_por_id(db, id, &ordem);
    if (resultado == RES_OK) {
        resultado = historico_listar_por_ordem(db, id, &lista, &quantidade);
    }
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhum evento no histórico.");
    } else {
        for (i = 0; i < quantidade; i++) {
            ordem_mostrar_evento(&lista[i]);
        }
    }
    free(lista);
}

void menu_ordens(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("ORDENS DE SERVIÇO");
        puts("1 - Abrir ordem\n2 - Consultar por ID\n3 - Listar todas");
        puts("4 - Listar por situação\n5 - Consultar histórico\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 5, &opcao);
        if (resultado != RES_OK) {
            return;
        }
        switch (opcao) {
            case 0: return;
            case 1: ordem_abrir_tela(db); break;
            case 2: ordem_consultar_tela(db); break;
            case 3: ordem_listar_tela(db, NULL); break;
            case 4: ordem_status_tela(db); break;
            case 5: ordem_historico_tela(db); break;
            default: break;
        }
    }
}
