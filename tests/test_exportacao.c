#include "clientes.h"
#include "equipamentos.h"
#include "exportacao.h"
#include "itens_ordem.h"
#include "ordens.h"
#include "pecas.h"
#include "relatorios.h"
#include "test_util.h"
#include "tecnicos.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_exportacao.sqlite"
#define CSV_RESUMO "build\\export_resumo.csv"
#define CSV_MES "build\\export_mes.csv"
#define CSV_ESTOQUE "build\\export_estoque.csv"
#define CSV_VAZIO "build\\export_vazio.csv"
#define CSV_MOVIMENTOS "build\\export_movimentos.csv"
#define CSV_INVALIDO "build\\export_invalido.csv"

static char *ler_arquivo(const char *caminho, size_t *tamanho)
{
    FILE *arquivo = fopen(caminho, "rb");
    char *conteudo;
    long bytes;

    if (arquivo == NULL) return NULL;
    if (fseek(arquivo, 0, SEEK_END) != 0 || (bytes = ftell(arquivo)) < 0 ||
        fseek(arquivo, 0, SEEK_SET) != 0) {
        (void)fclose(arquivo);
        return NULL;
    }
    conteudo = malloc((size_t)bytes + 1);
    if (conteudo == NULL) {
        (void)fclose(arquivo);
        return NULL;
    }
    if (fread(conteudo, 1, (size_t)bytes, arquivo) != (size_t)bytes ||
        ferror(arquivo)) {
        free(conteudo);
        (void)fclose(arquivo);
        return NULL;
    }
    if (fclose(arquivo) != 0) {
        free(conteudo);
        return NULL;
    }
    conteudo[bytes] = '\0';
    *tamanho = (size_t)bytes;
    return conteudo;
}

static int contar_linhas_csv(const char *conteudo)
{
    int linhas = 0;
    const char *cursor = conteudo;

    while ((cursor = strstr(cursor, "\r\n")) != NULL) {
        linhas++;
        cursor += 2;
    }
    return linhas;
}

static int csv_tem_finais_validos(const char *conteudo)
{
    const unsigned char *cursor = (const unsigned char *)conteudo;

    while (*cursor != '\0') {
        if (*cursor == '\n' && (cursor == (const unsigned char *)conteudo ||
                                cursor[-1] != '\r')) return 0;
        if (*cursor == '\n' && cursor > (const unsigned char *)conteudo &&
            cursor[-1] == '\r' && cursor > (const unsigned char *)conteudo + 1 &&
            cursor[-2] == '\r') return 0;
        cursor++;
    }
    return 1;
}

static int obter_linha(const char *conteudo, int indice, char *linha,
                       size_t tamanho)
{
    const char *inicio = conteudo;
    const char *fim;
    size_t comprimento;
    int atual = 0;

    if ((unsigned char)inicio[0] == 0xEF &&
        (unsigned char)inicio[1] == 0xBB &&
        (unsigned char)inicio[2] == 0xBF) {
        inicio += 3;
    }
    while (atual < indice) {
        fim = strstr(inicio, "\r\n");
        if (fim == NULL) return 0;
        inicio = fim + 2;
        atual++;
    }
    fim = strstr(inicio, "\r\n");
    if (fim == NULL) return 0;
    comprimento = (size_t)(fim - inicio);
    if (comprimento >= tamanho) return 0;
    memcpy(linha, inicio, comprimento);
    linha[comprimento] = '\0';
    return 1;
}

static int preparar_dados(sqlite3 *db, int *equipamento_id, int *tecnico_id,
                          int *peca_id, int *ordem_id)
{
    Cliente cliente = {0};
    Equipamento equipamento = {0};
    Tecnico tecnico = {0};
    Peca peca = {0};
    int cliente_id;

    snprintf(cliente.nome, sizeof(cliente.nome), "%s", "Cliente Exportacao");
    snprintf(cliente.documento, sizeof(cliente.documento), "%s",
             "12345678901");
    snprintf(cliente.telefone, sizeof(cliente.telefone), "%s", "11999999999");
    if (cliente_inserir(db, &cliente, &cliente_id) != RES_OK) return 0;
    equipamento.cliente_id = cliente_id;
    snprintf(equipamento.tipo, sizeof(equipamento.tipo), "%s", "NOTEBOOK");
    snprintf(equipamento.marca, sizeof(equipamento.marca), "%s", "Marca");
    snprintf(equipamento.modelo, sizeof(equipamento.modelo), "%s", "Modelo");
    snprintf(equipamento.numero_serie, sizeof(equipamento.numero_serie), "%s",
             "EXPORT-1");
    if (equipamento_inserir(db, &equipamento, equipamento_id) != RES_OK) {
        return 0;
    }
    snprintf(tecnico.nome, sizeof(tecnico.nome), "%s", "Tecnico Exportacao");
    snprintf(tecnico.especialidade, sizeof(tecnico.especialidade), "%s",
             "Geral");
    if (tecnico_inserir(db, &tecnico, tecnico_id) != RES_OK) return 0;
    snprintf(peca.codigo, sizeof(peca.codigo), "%s", "HDMI");
    snprintf(peca.nome, sizeof(peca.nome), "%s", "Cabo \"HDMI\"; 2m");
    peca.quantidade = 10;
    peca.estoque_minimo = 3;
    if (peca_cadastrar(db, &peca, peca_id) != RES_OK) return 0;
    return ordem_abrir(db, *equipamento_id, 0, "Ordem de exportacao",
                       ordem_id) == RES_OK &&
        ordem_atribuir_tecnico(db, *ordem_id, *tecnico_id) == RES_OK &&
        ordem_atualizar_status(db, *ordem_id, OS_EM_DIAGNOSTICO, NULL) ==
            RES_OK &&
        ordem_registrar_diagnostico(db, *ordem_id, "Diagnostico") == RES_OK &&
        ordem_atualizar_status(db, *ordem_id, OS_AGUARDANDO_APROVACAO, NULL)
            == RES_OK &&
        ordem_atualizar_status(db, *ordem_id, OS_APROVADA, NULL) == RES_OK;
}

static int criar_ordens_relatorio(sqlite3 *db, int equipamento_id)
{
    static const char *const status[9] = {
        "ABERTA", "EM_DIAGNOSTICO", "AGUARDANDO_APROVACAO", "APROVADA",
        "AGUARDANDO_PECA", "EM_REPARO", "CONCLUIDA", "ENTREGUE", "CANCELADA"
    };
    int i;

    for (i = 0; i < 9; i++) {
        int ordem_id;
        char data[20];
        sqlite3_stmt *consulta = NULL;
        int codigo;

        if (ordem_abrir(db, equipamento_id, 0, "Ordem relatorio",
                        &ordem_id) != RES_OK) return 0;
        snprintf(data, sizeof(data), "2026-%02d-15 12:00:00", i + 1);
        codigo = sqlite3_prepare_v2(
            db, "UPDATE ordens_servico SET status = ?1, data_abertura = ?2 "
            "WHERE id = ?3;", -1, &consulta, NULL);
        if (codigo == SQLITE_OK) {
            codigo = sqlite3_bind_text(consulta, 1, status[i], -1,
                                       SQLITE_TRANSIENT);
        }
        if (codigo == SQLITE_OK) {
            codigo = sqlite3_bind_text(consulta, 2, data, -1, SQLITE_TRANSIENT);
        }
        if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 3, ordem_id);
        if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
        if (consulta == NULL || sqlite3_finalize(consulta) != SQLITE_OK ||
            codigo != SQLITE_DONE) return 0;
    }
    return 1;
}

static void testar_bom_e_linhas(const char *caminho)
{
    char *conteudo;
    size_t tamanho = 0;

    conteudo = ler_arquivo(caminho, &tamanho);
    TESTE_VERIFICAR(conteudo != NULL && tamanho >= 3 &&
                    (unsigned char)conteudo[0] == 0xEF &&
                    (unsigned char)conteudo[1] == 0xBB &&
                    (unsigned char)conteudo[2] == 0xBF,
                    "arquivo inicia com BOM UTF-8");
    TESTE_VERIFICAR(conteudo != NULL && csv_tem_finais_validos(conteudo),
                    "linhas usam CRLF sem repeticao");
    free(conteudo);
}

static void testar_resumo(sqlite3 *db)
{
    char *conteudo;
    size_t tamanho = 0;

    TESTE_VERIFICAR(exportar_resumo_ordens(db, CSV_RESUMO) == RES_OK,
                    "exportar resumo de ordens");
    testar_bom_e_linhas(CSV_RESUMO);
    conteudo = ler_arquivo(CSV_RESUMO, &tamanho);
    TESTE_VERIFICAR(conteudo != NULL &&
                    contar_linhas_csv(conteudo) == 13,
                    "resumo tem cabecalho, situacoes e totais");
    TESTE_VERIFICAR(conteudo != NULL &&
                    strstr(conteudo, "Situacao;Quantidade\r\n") != NULL &&
                    strstr(conteudo, "Em diagnóstico;1\r\n") != NULL &&
                    strstr(conteudo, "Total;10\r\n") != NULL &&
                    strstr(conteudo, "Em aberto;7\r\n") != NULL &&
                    strstr(conteudo, "Concluidas;2\r\n") != NULL,
                    "resumo contem cabecalho e linhas finais UTF-8");
    free(conteudo);
}

static void testar_exportacao_mensal(sqlite3 *db)
{
    char *conteudo;
    size_t tamanho = 0;
    int matriz[12][OS_STATUS_TOTAL];
    FILE *arquivo;

    TESTE_VERIFICAR(exportar_ordens_por_mes(db, 2026, CSV_MES) == RES_OK,
                    "exportar ordens por mes");
    conteudo = ler_arquivo(CSV_MES, &tamanho);
    TESTE_VERIFICAR(conteudo != NULL &&
                    contar_linhas_csv(conteudo) == 14,
                    "relatorio mensal tem 14 linhas");
    TESTE_VERIFICAR(relatorio_ordens_por_mes(db, 2026, matriz) == RES_OK &&
                    conteudo != NULL &&
                    strstr(conteudo, "Jan;1;") != NULL &&
                    strstr(conteudo, "Fev;0;1;") != NULL &&
                    strstr(conteudo, "Jul;0;0;0;0;0;0;1;") != NULL &&
                    strstr(conteudo, "Set;0;0;0;0;0;0;0;0;1;") != NULL,
                    "valores mensais correspondem a matriz");
    free(conteudo);
    (void)remove(CSV_INVALIDO);
    TESTE_VERIFICAR(exportar_ordens_por_mes(db, 1999, CSV_INVALIDO) ==
                        RES_INVALIDO &&
                    exportar_ordens_por_mes(db, 2101, CSV_INVALIDO) ==
                        RES_INVALIDO,
                    "rejeitar anos invalidos");
    arquivo = fopen(CSV_INVALIDO, "rb");
    TESTE_VERIFICAR(arquivo == NULL, "ano invalido nao cria arquivo");
    if (arquivo != NULL) (void)fclose(arquivo);
}

static int criar_peca(sqlite3 *db, const char *codigo, const char *nome,
                      int quantidade, int minimo, int *id)
{
    Peca peca = {0};

    snprintf(peca.codigo, sizeof(peca.codigo), "%s", codigo);
    snprintf(peca.nome, sizeof(peca.nome), "%s", nome);
    peca.quantidade = quantidade;
    peca.estoque_minimo = minimo;
    return peca_cadastrar(db, &peca, id) == RES_OK;
}

static int ajustar_quantidade(sqlite3 *db, int peca_id, int quantidade)
{
    sqlite3_stmt *consulta = NULL;
    int codigo = sqlite3_exec(db, "PRAGMA ignore_check_constraints = ON;",
                              NULL, NULL, NULL);

    if (codigo == SQLITE_OK) {
        codigo = sqlite3_prepare_v2(db,
            "UPDATE pecas SET quantidade = ?1 WHERE id = ?2;",
            -1, &consulta, NULL);
    }
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 1, quantidade);
    if (codigo == SQLITE_OK) codigo = sqlite3_bind_int(consulta, 2, peca_id);
    if (codigo == SQLITE_OK) codigo = sqlite3_step(consulta);
    if (consulta != NULL && sqlite3_finalize(consulta) != SQLITE_OK) {
        codigo = SQLITE_ERROR;
    }
    if (sqlite3_exec(db, "PRAGMA ignore_check_constraints = OFF;",
                     NULL, NULL, NULL) != SQLITE_OK) return 0;
    return codigo == SQLITE_DONE;
}

static void testar_estoque_baixo(sqlite3 *db, int peca_id)
{
    char *conteudo;
    size_t tamanho = 0;
    int formula_id;
    int menos_id;

    TESTE_VERIFICAR(criar_peca(db, "FORMULA", "=SOMA(A1)", 0, 1,
                               &formula_id) &&
                    criar_peca(db, "MENOS", "-teste", 0, 1, &menos_id),
                    "criar pecas para testar formulas");
    TESTE_VERIFICAR(ajustar_quantidade(db, peca_id, 2) &&
                    exportar_estoque_baixo(db, CSV_ESTOQUE) == RES_OK,
                    "exportar pecas com estoque baixo");
    conteudo = ler_arquivo(CSV_ESTOQUE, &tamanho);
    TESTE_VERIFICAR(conteudo != NULL &&
                    strstr(conteudo, "\"Cabo \"\"HDMI\"\"; 2m\"") != NULL,
                    "escapar aspas e separador no nome");
    TESTE_VERIFICAR(conteudo != NULL &&
                    strstr(conteudo, "'=SOMA(A1)") != NULL &&
                    strstr(conteudo, "'-teste") != NULL,
                    "neutralizar formulas em texto");
    free(conteudo);
    TESTE_VERIFICAR(ajustar_quantidade(db, peca_id, -1),
                    "preparar numero negativo somente no teste");
    TESTE_VERIFICAR(exportar_estoque_baixo(db, CSV_ESTOQUE) == RES_OK,
                    "exportar valor numerico negativo");
    conteudo = ler_arquivo(CSV_ESTOQUE, &tamanho);
    TESTE_VERIFICAR(conteudo != NULL && strstr(conteudo, ";-1;3\r\n") != NULL &&
                    strstr(conteudo, ";'-1;3\r\n") == NULL,
                    "numero negativo nao recebe apostrofo");
    free(conteudo);
    TESTE_VERIFICAR(ajustar_quantidade(db, peca_id, 10),
                    "restaurar estoque de teste");
    TESTE_VERIFICAR(peca_registrar_entrada(db, formula_id, 2, NULL) == RES_OK &&
                    peca_registrar_entrada(db, menos_id, 2, NULL) == RES_OK,
                    "deixar todas as pecas acima do minimo");
    TESTE_VERIFICAR(exportar_estoque_baixo(db, CSV_VAZIO) == RES_OK,
                    "exportar estoque baixo vazio");
    conteudo = ler_arquivo(CSV_VAZIO, &tamanho);
    TESTE_VERIFICAR(conteudo != NULL && contar_linhas_csv(conteudo) == 1,
                    "estoque vazio contem apenas cabecalho");
    free(conteudo);
}

static void testar_movimentacoes(sqlite3 *db, int peca_id, int ordem_id)
{
    char *conteudo;
    char linha[512];
    size_t tamanho = 0;

    TESTE_VERIFICAR(peca_registrar_saida(db, peca_id, 1, "Manutencao") ==
                        RES_OK &&
                    item_ordem_usar_peca(db, ordem_id, peca_id, 2) == RES_OK,
                    "criar saidas manual e vinculada a OS");
    TESTE_VERIFICAR(exportar_movimentacoes(db, CSV_MOVIMENTOS) == RES_OK,
                    "exportar movimentacoes");
    conteudo = ler_arquivo(CSV_MOVIMENTOS, &tamanho);
    TESTE_VERIFICAR(conteudo != NULL &&
                    contar_linhas_csv(conteudo) == 6,
                    "exportar todas as movimentacoes");
    TESTE_VERIFICAR(conteudo != NULL &&
                    obter_linha(conteudo, 4, linha, sizeof(linha)) &&
                    strstr(linha, ";SAIDA;1;;Manutencao") != NULL &&
                    obter_linha(conteudo, 5, linha, sizeof(linha)) &&
                    strstr(linha, ";SAIDA;2;") != NULL,
                    "ordem vazia manual e preenchida para OS");
    free(conteudo);
}

int main(void)
{
    sqlite3 *db = NULL;
    int equipamento_id = 0;
    int tecnico_id = 0;
    int peca_id = 0;
    int ordem_id = 0;

    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        TESTE_VERIFICAR(0, "remover banco anterior");
        return teste_codigo_saida();
    }
    if (db_abrir(&db, CAMINHO_BANCO) != RES_OK) {
        TESTE_VERIFICAR(0, "abrir banco de exportacao");
        return teste_codigo_saida();
    }
    TESTE_VERIFICAR(preparar_dados(db, &equipamento_id, &tecnico_id,
                                   &peca_id, &ordem_id),
                    "preparar cliente, equipamento, tecnico, peca e OS");
    TESTE_VERIFICAR(criar_ordens_relatorio(db, equipamento_id),
                    "criar ordens para os relatorios");
    testar_resumo(db);
    testar_exportacao_mensal(db);
    testar_estoque_baixo(db, peca_id);
    testar_movimentacoes(db, peca_id, ordem_id);
    TESTE_VERIFICAR(exportar_resumo_ordens(db, "build\\inexistente\\x.csv") ==
                        RES_ERRO_ARQUIVO,
                    "caminho invalido retorna erro de arquivo");
    db_fechar(db);
    return teste_codigo_saida();
}
