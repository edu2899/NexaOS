#include "clientes.h"
#include "equipamentos.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_equipamentos.sqlite"

static int criar_cliente(sqlite3 *db, const char *nome, const char *documento,
                         int *id)
{
    Cliente cliente = {0};

    snprintf(cliente.nome, sizeof(cliente.nome), "%s", nome);
    snprintf(cliente.documento, sizeof(cliente.documento), "%s", documento);
    snprintf(cliente.telefone, sizeof(cliente.telefone), "%s", "11999999999");
    return cliente_inserir(db, &cliente, id) == RES_OK;
}

static void preparar_equipamento(Equipamento *equipamento, int cliente_id,
                                 const char *tipo, const char *marca,
                                 const char *modelo, const char *serie)
{
    memset(equipamento, 0, sizeof(*equipamento));
    equipamento->cliente_id = cliente_id;
    snprintf(equipamento->tipo, sizeof(equipamento->tipo), "%s", tipo);
    snprintf(equipamento->marca, sizeof(equipamento->marca), "%s", marca);
    snprintf(equipamento->modelo, sizeof(equipamento->modelo), "%s", modelo);
    snprintf(equipamento->numero_serie, sizeof(equipamento->numero_serie),
             "%s", serie);
}

static void testar_lista_vazia(sqlite3 *db)
{
    Equipamento *lista = (Equipamento *)1;
    int quantidade = -1;
    Resultado resultado = equipamento_listar(db, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && lista == NULL && quantidade == 0,
                    "listar sem equipamentos");
    free(lista);
}

static void testar_validacoes(sqlite3 *db, const Equipamento *modelo)
{
    Equipamento invalido = *modelo;
    int id_novo = 0;

    snprintf(invalido.tipo, sizeof(invalido.tipo), "%s", "TABLET");
    TESTE_VERIFICAR(equipamento_inserir(db, &invalido, &id_novo) ==
                        RES_INVALIDO,
                    "rejeitar tipo invalido");
    invalido = *modelo;
    invalido.marca[0] = '\0';
    TESTE_VERIFICAR(equipamento_inserir(db, &invalido, &id_novo) ==
                        RES_INVALIDO,
                    "rejeitar marca vazia");
    invalido = *modelo;
    invalido.modelo[0] = '\0';
    TESTE_VERIFICAR(equipamento_inserir(db, &invalido, &id_novo) ==
                        RES_INVALIDO,
                    "rejeitar modelo vazio");
    invalido = *modelo;
    invalido.numero_serie[0] = '\0';
    TESTE_VERIFICAR(equipamento_inserir(db, &invalido, &id_novo) ==
                        RES_INVALIDO,
                    "rejeitar serie vazia");
}

static void testar_listas(sqlite3 *db, const int *ids, int id_cliente_a,
                          int id_cliente_b, int id_cliente_vazio)
{
    Equipamento *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = equipamento_listar(db, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 3,
                    "listar tres equipamentos");
    if (resultado == RES_OK && quantidade == 3) {
        for (i = 0; i < quantidade; i++) {
            TESTE_VERIFICAR(lista[i].id == ids[i],
                            "equipamentos em ordem de id");
        }
    }
    free(lista);

    lista = NULL;
    quantidade = 0;
    resultado = equipamento_listar_por_cliente(db, id_cliente_a, &lista,
                                                 &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 2,
                    "listar equipamentos do primeiro cliente");
    if (resultado == RES_OK && quantidade == 2) {
        TESTE_VERIFICAR(lista[0].cliente_id == id_cliente_a &&
                        lista[1].cliente_id == id_cliente_a,
                        "filtro retorna somente primeiro cliente");
    }
    free(lista);

    lista = NULL;
    quantidade = -1;
    resultado = equipamento_listar_por_cliente(db, id_cliente_b, &lista,
                                                 &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 1 &&
                    lista != NULL && lista[0].cliente_id == id_cliente_b,
                    "listar equipamento do segundo cliente");
    free(lista);

    lista = (Equipamento *)1;
    quantidade = -1;
    resultado = equipamento_listar_por_cliente(db, id_cliente_vazio, &lista,
                                                 &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 0 && lista == NULL,
                    "cliente sem equipamentos retorna lista vazia");
    free(lista);
}

int main(void)
{
    sqlite3 *db = NULL;
    Equipamento primeiro;
    Equipamento segundo;
    Equipamento terceiro;
    Equipamento consultado;
    Equipamento *lista = NULL;
    int cliente_a;
    int cliente_b;
    int id_primeiro = 0;
    int id_segundo = 0;
    int id_terceiro = 0;
    int id_duplicado = 0;
    int quantidade;
    int ids[3];
    Resultado resultado;

    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        perror(CAMINHO_BANCO);
        TESTE_VERIFICAR(0, "apagar banco de teste anterior");
        return teste_codigo_saida();
    }
    resultado = db_abrir(&db, CAMINHO_BANCO);
    TESTE_VERIFICAR(resultado == RES_OK, "abrir banco de equipamentos");
    if (resultado != RES_OK) {
        return teste_codigo_saida();
    }

    TESTE_VERIFICAR(criar_cliente(db, "Cliente A", "11111111111",
                                  &cliente_a),
                    "criar primeiro cliente");
    TESTE_VERIFICAR(criar_cliente(db, "Cliente B", "22222222222",
                                  &cliente_b),
                    "criar segundo cliente");
    testar_lista_vazia(db);

    preparar_equipamento(&primeiro, cliente_a, "COMPUTADOR", "Marca A",
                         "Modelo A", "sn123");
    resultado = equipamento_inserir(db, &primeiro, &id_primeiro);
    TESTE_VERIFICAR(resultado == RES_OK && id_primeiro > 0,
                    "inserir equipamento valido");
    resultado = equipamento_buscar_por_id(db, id_primeiro, &consultado);
    TESTE_VERIFICAR(resultado == RES_OK &&
                    consultado.cliente_id == cliente_a &&
                    strcmp(consultado.cliente_nome, "Cliente A") == 0 &&
                    strcmp(consultado.tipo, "COMPUTADOR") == 0 &&
                    strcmp(consultado.marca, "Marca A") == 0 &&
                    strcmp(consultado.modelo, "Modelo A") == 0 &&
                    strcmp(consultado.numero_serie, "SN123") == 0,
                    "buscar dados e serie normalizada");

    primeiro.cliente_id = 999999;
    TESTE_VERIFICAR(equipamento_inserir(db, &primeiro, &id_duplicado) ==
                        RES_NAO_ENCONTRADO,
                    "rejeitar cliente inexistente");
    primeiro.cliente_id = cliente_a;

    preparar_equipamento(&segundo, cliente_a, "NOTEBOOK", "Marca B",
                         "Modelo B", "SN123");
    TESTE_VERIFICAR(equipamento_inserir(db, &segundo, &id_duplicado) ==
                        RES_DUPLICADO,
                    "serie duplicada ignorando maiusculas");
    testar_validacoes(db, &segundo);

    preparar_equipamento(&segundo, cliente_b, "NOTEBOOK", "Marca B",
                         "Modelo B", "NB-456");
    resultado = equipamento_inserir(db, &segundo, &id_segundo);
    TESTE_VERIFICAR(resultado == RES_OK, "inserir segundo equipamento");
    preparar_equipamento(&terceiro, cliente_a, "IMPRESSORA", "Marca C",
                         "Modelo C", "IMP-789");
    resultado = equipamento_inserir(db, &terceiro, &id_terceiro);
    TESTE_VERIFICAR(resultado == RES_OK, "inserir terceiro equipamento");

    TESTE_VERIFICAR(equipamento_buscar_por_id(db, 999999, &consultado) ==
                        RES_NAO_ENCONTRADO,
                    "buscar equipamento inexistente");
    ids[0] = id_primeiro;
    ids[1] = id_segundo;
    ids[2] = id_terceiro;
    testar_listas(db, ids, cliente_a, cliente_b, 999998);

    resultado = equipamento_listar(db, &lista, &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 3,
                    "listar antes da persistencia");
    free(lista);

    db_fechar(db);
    db = NULL;
    resultado = db_abrir(&db, CAMINHO_BANCO);
    TESTE_VERIFICAR(resultado == RES_OK, "reabrir banco de equipamentos");
    if (resultado == RES_OK) {
        TESTE_VERIFICAR(
            equipamento_buscar_por_id(db, id_primeiro, &consultado) == RES_OK
                && strcmp(consultado.numero_serie, "SN123") == 0,
            "equipamento persiste apos reabrir");
    }

    db_fechar(db);
    return teste_codigo_saida();
}
