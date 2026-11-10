#include "tecnicos.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_tecnicos.sqlite"

static void preparar_tecnico(Tecnico *tecnico, const char *nome,
                             const char *especialidade)
{
    memset(tecnico, 0, sizeof(*tecnico));
    snprintf(tecnico->nome, sizeof(tecnico->nome), "%s", nome);
    snprintf(tecnico->especialidade, sizeof(tecnico->especialidade), "%s",
             especialidade);
}

static int tecnico_tem_dados(const Tecnico *tecnico, const char *nome,
                             const char *especialidade)
{
    return strcmp(tecnico->nome, nome) == 0 &&
           strcmp(tecnico->especialidade, especialidade) == 0;
}

static int testar_lista_vazia(sqlite3 *db)
{
    Tecnico *lista = (Tecnico *)1;
    int quantidade = -1;
    Resultado resultado = tecnico_listar(db, &lista, &quantidade);
    int passou = resultado == RES_OK && quantidade == 0 && lista == NULL;

    TESTE_VERIFICAR(passou, "listar banco sem tecnicos");
    free(lista);
    return passou;
}

static void testar_validacoes(sqlite3 *db, const Tecnico *modelo)
{
    Tecnico invalido = *modelo;
    int id_novo = 0;

    invalido.nome[0] = '\0';
    TESTE_VERIFICAR(tecnico_inserir(db, &invalido, &id_novo) == RES_INVALIDO,
                    "rejeitar nome vazio");
    invalido = *modelo;
    invalido.especialidade[0] = '\0';
    TESTE_VERIFICAR(tecnico_inserir(db, &invalido, &id_novo) == RES_INVALIDO,
                    "rejeitar especialidade vazia");
}

static void testar_listagem(sqlite3 *db, const int ids[3])
{
    Tecnico *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = tecnico_listar(db, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 3,
                    "listar tres tecnicos");
    if (resultado == RES_OK && quantidade == 3) {
        for (i = 0; i < quantidade; i++) {
            TESTE_VERIFICAR(lista[i].id == ids[i],
                            "tecnicos em ordem de id");
        }
    }
    free(lista);
}

int main(void)
{
    vazamento_iniciar();
    sqlite3 *db = NULL;
    Tecnico primeiro;
    Tecnico segundo;
    Tecnico terceiro;
    Tecnico consultado;
    int id_primeiro = 0;
    int id_segundo = 0;
    int id_terceiro = 0;
    int id_duplicado = 0;
    int ids[3];
    Resultado resultado;

    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        perror(CAMINHO_BANCO);
        TESTE_VERIFICAR(0, "apagar banco de teste anterior");
        return teste_codigo_saida();
    }
    resultado = db_abrir(&db, CAMINHO_BANCO);
    TESTE_VERIFICAR(resultado == RES_OK, "abrir banco de tecnicos");
    if (resultado != RES_OK) {
        return teste_codigo_saida();
    }
    if (!testar_lista_vazia(db)) {
        db_fechar(db);
        return teste_codigo_saida();
    }

    preparar_tecnico(&primeiro, "Ana Souza", "Hardware");
    resultado = tecnico_inserir(db, &primeiro, &id_primeiro);
    TESTE_VERIFICAR(resultado == RES_OK && id_primeiro > 0,
                    "inserir tecnico valido");
    resultado = tecnico_buscar_por_id(db, id_primeiro, &consultado);
    TESTE_VERIFICAR(resultado == RES_OK &&
                    tecnico_tem_dados(&consultado, "Ana Souza", "Hardware"),
                    "buscar dados do tecnico");

    testar_validacoes(db, &primeiro);
    TESTE_VERIFICAR(tecnico_inserir(db, &primeiro, &id_duplicado) ==
                        RES_DUPLICADO,
                    "rejeitar nome repetido");
    TESTE_VERIFICAR(tecnico_buscar_por_id(db, 999999, &consultado) ==
                        RES_NAO_ENCONTRADO,
                    "buscar id inexistente");

    preparar_tecnico(&segundo, "Bruno Lima", "Redes");
    preparar_tecnico(&terceiro, "Carla Dias", "Impressoras");
    TESTE_VERIFICAR(tecnico_inserir(db, &segundo, &id_segundo) == RES_OK,
                    "inserir segundo tecnico");
    TESTE_VERIFICAR(tecnico_inserir(db, &terceiro, &id_terceiro) == RES_OK,
                    "inserir terceiro tecnico");
    ids[0] = id_primeiro;
    ids[1] = id_segundo;
    ids[2] = id_terceiro;
    testar_listagem(db, ids);

    db_fechar(db);
    db = NULL;
    resultado = db_abrir(&db, CAMINHO_BANCO);
    TESTE_VERIFICAR(resultado == RES_OK, "reabrir banco de tecnicos");
    if (resultado == RES_OK) {
        resultado = tecnico_buscar_por_id(db, id_primeiro, &consultado);
        TESTE_VERIFICAR(resultado == RES_OK &&
                        tecnico_tem_dados(&consultado, "Ana Souza",
                                          "Hardware"),
                        "tecnico persiste apos reabrir");
    }

    db_fechar(db);
    return teste_codigo_saida();
}
