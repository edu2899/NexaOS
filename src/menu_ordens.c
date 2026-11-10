#include "menu_ordens.h"

#include "clientes.h"
#include "equipamentos.h"
#include "historico.h"
#include "menu_itens_ordem.h"
#include "ordens.h"
#include "tecnicos.h"
#include "util.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void ordem_imprimir_linha(const OrdemServico *ordem)
{
    char equipamento[124];
    char id[16];
    char data[20];
    int status = os_status_de_codigo(ordem->status);

    snprintf(id, sizeof(id), "%d", ordem->id);
    snprintf(equipamento, sizeof(equipamento), "%s %s", ordem->equip_marca,
             ordem->equip_modelo);
    util_formatar_data(ordem->data_abertura, data, sizeof(data));
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

static void ordem_listar_tela(sqlite3 *db, const char *status)
{
    OrdemServico *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = ordem_listar(db, status, &lista, &quantidade);

    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
    } else if (quantidade == 0) {
        puts("Nenhuma ordem encontrada.");
    } else {
        ordem_imprimir_cabecalho();
        for (i = 0; i < quantidade; i++) {
            ordem_imprimir_linha(&lista[i]);
        }
    }
    free(lista);
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

static int ordem_ler_e_mostrar(sqlite3 *db, int *id, OrdemServico *ordem)
{
    Resultado resultado;

    if (!ordem_ler_id("ID da ordem: ", id, 0)) return 0;
    resultado = ordem_buscar_por_id(db, *id, ordem);
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return 0;
    }
    ordem_mostrar_dados(ordem);
    return 1;
}

static int ordem_confirmar_alteracao(const char *mensagem)
{
    int confirmado = 0;
    Resultado resultado = util_ler_confirmacao(mensagem, &confirmado);

    return resultado == RES_OK && confirmado;
}

static void ordem_atribuir_tecnico_tela(sqlite3 *db)
{
    OrdemServico ordem;
    int ordem_id;
    int tecnico_id;
    Resultado resultado;

    if (!ordem_ler_e_mostrar(db, &ordem_id, &ordem)) return;
    ordem_mostrar_tecnicos(db);
    if (!ordem_ler_id("ID do técnico: ", &tecnico_id, 0)) return;
    if (!ordem_confirmar_alteracao("Confirmar atribuição? (S/N): ")) return;
    resultado = ordem_atribuir_tecnico(db, ordem_id, tecnico_id);
    if (resultado == RES_OK) puts("[OK] Técnico atribuído.");
    else util_erro(util_texto_resultado(resultado));
}

static void ordem_diagnostico_tela(sqlite3 *db)
{
    OrdemServico ordem;
    char diagnostico[501];
    int ordem_id;
    Resultado resultado;

    if (!ordem_ler_e_mostrar(db, &ordem_id, &ordem)) return;
    printf("Diagnóstico atual: %s\n",
           ordem.diagnostico[0] == '\0' ? "-" : ordem.diagnostico);
    resultado = util_ler_texto_obrigatorio("Novo diagnóstico: ", diagnostico,
                                            sizeof(diagnostico));
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    if (!ordem_confirmar_alteracao("Confirmar diagnóstico? (S/N): ")) return;
    resultado = ordem_registrar_diagnostico(db, ordem_id, diagnostico);
    if (resultado == RES_OK) puts("[OK] Diagnóstico registrado.");
    else util_erro(util_texto_resultado(resultado));
}

static void ordem_observacao_tela(sqlite3 *db)
{
    OrdemServico ordem;
    char observacao[501];
    int ordem_id;
    Resultado resultado;

    if (!ordem_ler_e_mostrar(db, &ordem_id, &ordem)) return;
    resultado = util_ler_texto_obrigatorio("Observação: ", observacao,
                                            sizeof(observacao));
    if (resultado == RES_OK) {
        resultado = ordem_registrar_observacao(db, ordem_id, observacao);
    }
    if (resultado == RES_OK) puts("[OK] Observação registrada.");
    else util_erro(util_texto_resultado(resultado));
}

static int ordem_escolher_destino(StatusOS atual, StatusOS *destino)
{
    StatusOS opcoes[OS_STATUS_TOTAL];
    int quantidade = 0;
    int i;
    int escolha;
    Resultado resultado;

    for (i = 0; i < OS_STATUS_TOTAL; i++) {
        if (ordem_transicao_permitida(atual, (StatusOS)i)) {
            opcoes[quantidade] = (StatusOS)i;
            printf("%d - %s\n", ++quantidade, os_status_rotulo((StatusOS)i));
        }
    }
    if (quantidade == 0) {
        puts("Esta ordem não aceita novas mudanças de situação.");
        return 0;
    }
    resultado = util_ler_inteiro("Escolha a nova situação: ", 1, quantidade,
                                 &escolha);
    if (resultado != RES_OK) return 0;
    *destino = opcoes[escolha - 1];
    return 1;
}

static void ordem_status_atualizar_tela(sqlite3 *db)
{
    OrdemServico ordem;
    StatusOS atual;
    StatusOS novo_status;
    char observacao[501] = "";
    int ordem_id;
    Resultado resultado;

    if (!ordem_ler_e_mostrar(db, &ordem_id, &ordem)) return;
    atual = (StatusOS)os_status_de_codigo(ordem.status);
    if (!ordem_escolher_destino(atual, &novo_status)) return;
    resultado = util_ler_linha("Observação (Enter para pular): ",
                               observacao, sizeof(observacao));
    if (resultado != RES_OK) {
        util_erro(util_texto_resultado(resultado));
        return;
    }
    if (novo_status == OS_CANCELADA && observacao[0] == '\0') {
        util_erro("Informe o motivo do cancelamento.");
        return;
    }
    if (!ordem_confirmar_alteracao("Confirmar mudança? (S/N): ")) return;
    resultado = ordem_atualizar_status(db, ordem_id, novo_status,
                                       observacao[0] == '\0' ? NULL : observacao);
    if (resultado == RES_OK) puts("[OK] Situação atualizada.");
    else util_erro(util_texto_resultado(resultado));
}

void menu_ordens(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        util_titulo("ORDENS DE SERVIÇO");
        puts("1 - Abrir ordem\n2 - Consultar por ID\n3 - Listar todas");
        puts("4 - Listar por situação\n5 - Consultar histórico");
        puts("6 - Atribuir técnico\n7 - Registrar diagnóstico");
        puts("8 - Registrar observação\n9 - Atualizar situação");
        puts("10 - Peças da ordem\n0 - Voltar");
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 10, &opcao);
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
            case 6: ordem_atribuir_tecnico_tela(db); break;
            case 7: ordem_diagnostico_tela(db); break;
            case 8: ordem_observacao_tela(db); break;
            case 9: ordem_status_atualizar_tela(db); break;
            case 10: menu_itens_ordem(db); break;
            default: break;
        }
    }
}
