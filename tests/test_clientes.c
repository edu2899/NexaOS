#include "clientes.h"
#include "test_util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAMINHO_BANCO "build\\test_clientes.sqlite"

static void preparar_cliente(Cliente *cliente, const char *nome,
                             const char *documento, const char *telefone)
{
    memset(cliente, 0, sizeof(*cliente));
    snprintf(cliente->nome, sizeof(cliente->nome), "%s", nome);
    snprintf(cliente->documento, sizeof(cliente->documento), "%s", documento);
    snprintf(cliente->telefone, sizeof(cliente->telefone), "%s", telefone);
}

static int cliente_tem_dados(const Cliente *cliente, const char *nome,
                             const char *documento, const char *telefone,
                             const char *email, const char *endereco)
{
    return strcmp(cliente->nome, nome) == 0 &&
           strcmp(cliente->documento, documento) == 0 &&
           strcmp(cliente->telefone, telefone) == 0 &&
           strcmp(cliente->email, email) == 0 &&
           strcmp(cliente->endereco, endereco) == 0;
}

static int preparar_banco(sqlite3 **db)
{
    if (remove(CAMINHO_BANCO) != 0 && errno != ENOENT) {
        perror(CAMINHO_BANCO);
        TESTE_VERIFICAR(0, "apagar banco de teste anterior");
        return 0;
    }

    TESTE_VERIFICAR(db_abrir(db, CAMINHO_BANCO) == RES_OK,
                    "abrir banco de teste");
    return *db != NULL;
}

static void testar_lista_vazia(sqlite3 *db)
{
    Cliente *lista = (Cliente *)1;
    int quantidade = -1;
    Resultado resultado = cliente_listar(db, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 0 && lista == NULL,
                    "listar banco vazio");
    free(lista);
}

static void testar_validacoes(sqlite3 *db, const Cliente *modelo)
{
    Cliente invalido = *modelo;
    int id_novo = 0;

    invalido.nome[0] = '\0';
    TESTE_VERIFICAR(cliente_inserir(db, &invalido, &id_novo) == RES_INVALIDO,
                    "rejeitar nome vazio");
    invalido = *modelo;
    snprintf(invalido.documento, sizeof(invalido.documento), "%s", "123");
    TESTE_VERIFICAR(cliente_inserir(db, &invalido, &id_novo) == RES_INVALIDO,
                    "rejeitar documento invalido");
    invalido = *modelo;
    snprintf(invalido.telefone, sizeof(invalido.telefone), "%s", "123");
    TESTE_VERIFICAR(cliente_inserir(db, &invalido, &id_novo) == RES_INVALIDO,
                    "rejeitar telefone invalido");
    invalido = *modelo;
    snprintf(invalido.email, sizeof(invalido.email), "%s", "email-invalido");
    TESTE_VERIFICAR(cliente_inserir(db, &invalido, &id_novo) == RES_INVALIDO,
                    "rejeitar email invalido");
}

static void testar_atualizacao(sqlite3 *db, Cliente *cliente,
                               const Cliente *outro)
{
    Cliente consultado;
    Cliente alterado = *cliente;
    Resultado resultado;

    snprintf(alterado.nome, sizeof(alterado.nome), "%s", "Nome atualizado");
    snprintf(alterado.telefone, sizeof(alterado.telefone), "%s", "11988887777");
    snprintf(alterado.email, sizeof(alterado.email), "%s", "novo@exemplo.com");
    snprintf(alterado.endereco, sizeof(alterado.endereco), "%s", "Rua Nova, 10");
    resultado = cliente_atualizar(db, &alterado);
    TESTE_VERIFICAR(resultado == RES_OK, "atualizar cliente");
    resultado = cliente_buscar_por_id(db, alterado.id, &consultado);
    TESTE_VERIFICAR(resultado == RES_OK &&
                    cliente_tem_dados(&consultado, "Nome atualizado",
                                      cliente->documento, "11988887777",
                                      "novo@exemplo.com", "Rua Nova, 10"),
                    "busca reflete atualizacao");
    *cliente = alterado;

    alterado.id = 999999;
    TESTE_VERIFICAR(cliente_atualizar(db, &alterado) == RES_NAO_ENCONTRADO,
                    "atualizar id inexistente");

    alterado = *cliente;
    snprintf(alterado.documento, sizeof(alterado.documento), "%s",
             outro->documento);
    TESTE_VERIFICAR(cliente_atualizar(db, &alterado) == RES_DUPLICADO,
                    "atualizar com documento de outro cliente");
    TESTE_VERIFICAR(cliente_atualizar(db, cliente) == RES_OK,
                    "atualizar mantendo o proprio documento");
}

static void testar_listagem(sqlite3 *db, const int *ids, int esperados)
{
    Cliente *lista = NULL;
    int quantidade = 0;
    int i;
    Resultado resultado = cliente_listar(db, &lista, &quantidade);

    TESTE_VERIFICAR(resultado == RES_OK && quantidade == esperados,
                    "listar quantidade esperada");
    if (resultado == RES_OK && quantidade == esperados) {
        for (i = 0; i < quantidade; i++) {
            TESTE_VERIFICAR(lista[i].id == ids[i], "listagem ordenada por id");
        }
    }
    free(lista);
}

int main(void)
{
    vazamento_iniciar();
    sqlite3 *db = NULL;
    Cliente primeiro;
    Cliente segundo;
    Cliente terceiro;
    Cliente consultado;
    Cliente *lista = NULL;
    int id_primeiro = 0;
    int id_segundo = 0;
    int id_terceiro = 0;
    int id_injecao = 0;
    int quantidade = 0;
    int ids[3];
    Resultado resultado;

    if (!preparar_banco(&db)) {
        return teste_codigo_saida();
    }
    testar_lista_vazia(db);

    preparar_cliente(&primeiro, "Ana Silva", "12345678901", "11999999999");
    snprintf(primeiro.email, sizeof(primeiro.email), "%s", "ana@exemplo.com");
    snprintf(primeiro.endereco, sizeof(primeiro.endereco), "%s",
             "Rua Central, 20");
    resultado = cliente_inserir(db, &primeiro, &id_primeiro);
    TESTE_VERIFICAR(resultado == RES_OK && id_primeiro > 0,
                    "inserir cliente valido");
    resultado = cliente_buscar_por_id(db, id_primeiro, &consultado);
    TESTE_VERIFICAR(resultado == RES_OK &&
                    cliente_tem_dados(&consultado, "Ana Silva", "12345678901",
                                      "11999999999", "ana@exemplo.com",
                                      "Rua Central, 20") &&
                    consultado.data_cadastro[0] != '\0',
                    "buscar dados inseridos e data de cadastro");
    primeiro.id = id_primeiro;

    preparar_cliente(&segundo, "Bruno Lima", "12345678000199", "1133334444");
    resultado = cliente_inserir(db, &segundo, &id_segundo);
    TESTE_VERIFICAR(resultado == RES_OK && id_segundo > id_primeiro,
                    "inserir cliente sem email e endereco");
    resultado = cliente_buscar_por_id(db, id_segundo, &consultado);
    TESTE_VERIFICAR(resultado == RES_OK && consultado.email[0] == '\0' &&
                    consultado.endereco[0] == '\0',
                    "campos opcionais vazios voltam vazios");
    segundo.id = id_segundo;

    {
        Cliente duplicado = segundo;
        int id_duplicado = 0;

        snprintf(duplicado.documento, sizeof(duplicado.documento), "%s",
                 primeiro.documento);
        TESTE_VERIFICAR(cliente_inserir(db, &duplicado, &id_duplicado) ==
                            RES_DUPLICADO,
                        "rejeitar documento duplicado");
    }
    testar_validacoes(db, &primeiro);

    preparar_cliente(&terceiro, "Carla Souza", "98765432100", "11912345678");
    TESTE_VERIFICAR(cliente_inserir(db, &terceiro, &id_terceiro) == RES_OK,
                    "inserir terceiro cliente");
    terceiro.id = id_terceiro;

    TESTE_VERIFICAR(cliente_buscar_por_id(db, 999999, &consultado) ==
                        RES_NAO_ENCONTRADO,
                    "buscar id inexistente");
    testar_atualizacao(db, &primeiro, &segundo);

    ids[0] = id_primeiro;
    ids[1] = id_segundo;
    ids[2] = id_terceiro;
    testar_listagem(db, ids, 3);

    preparar_cliente(&consultado, "Robert'); DROP TABLE clientes;--",
                     "11122233344", "11955556666");
    resultado = cliente_inserir(db, &consultado, &id_injecao);
    TESTE_VERIFICAR(resultado == RES_OK, "inserir texto semelhante a SQL");
    resultado = cliente_buscar_por_id(db, id_injecao, &consultado);
    TESTE_VERIFICAR(resultado == RES_OK &&
                    strcmp(consultado.nome,
                           "Robert'); DROP TABLE clientes;--") == 0,
                    "texto de entrada gravado literalmente");
    resultado = cliente_listar(db, &lista, &quantidade);
    TESTE_VERIFICAR(resultado == RES_OK && quantidade == 4,
                    "tabela clientes continua disponivel");
    free(lista);

    db_fechar(db);
    db = NULL;
    resultado = db_abrir(&db, CAMINHO_BANCO);
    TESTE_VERIFICAR(resultado == RES_OK, "reabrir banco persistido");
    if (resultado == RES_OK) {
        TESTE_VERIFICAR(
            cliente_buscar_por_id(db, id_primeiro, &consultado) == RES_OK &&
            strcmp(consultado.nome, "Nome atualizado") == 0,
            "cliente persiste apos reabrir banco");
    }

    db_fechar(db);
    return teste_codigo_saida();
}
