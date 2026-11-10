/*
 * Inicializa o banco e direciona o usuario aos menus do sistema.
 * As regras de negocio ficam nos modulos especializados.
 */
#include "db.h"
#include "clientes.h"
#include "equipamentos.h"
#include "tecnicos.h"
#include "menu_ordens.h"
#include "menu_estoque.h"
#include "menu_relatorios.h"
#include "menu_exportacao.h"
#include "util.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void exibir_menu_principal(void)
{
    util_titulo("NEXAOS");
    printf("\n");
    printf("1 - Gerenciar clientes\n");
    printf("2 - Gerenciar equipamentos\n");
    printf("3 - Gerenciar técnicos\n");
    printf("4 - Ordens de serviço\n");
    printf("5 - Controle de estoque\n");
    printf("6 - Relatórios\n");
    printf("7 - Exportar dados\n");
    printf("0 - Sair\n\n");
}

static void executar_opcao(sqlite3 *db, int opcao)
{
    switch (opcao) {
        case 1: menu_clientes(db); break;
        case 2: menu_equipamentos(db); break;
        case 3: menu_tecnicos(db); break;
        case 4: menu_ordens(db); break;
        case 5: menu_estoque(db); break;
        case 6: menu_relatorios(db); break;
        case 7: menu_exportacao(db); break;
        default: break;
    }
}

static void executar_menu_principal(sqlite3 *db)
{
    int opcao;
    Resultado resultado;

    while (1) {
        exibir_menu_principal();
        resultado = util_ler_inteiro("Escolha uma opção: ", 0, 7, &opcao);
        if (resultado == RES_ERRO_ARQUIVO) {
            break;
        }
        if (resultado != RES_OK) {
            util_erro(util_texto_resultado(resultado));
            continue;
        }

        if (opcao == 0) return;
        executar_opcao(db, opcao);
    }
}

/* Inicializa o banco e executa o menu principal. */
int main(void)
{
    sqlite3 *db = NULL;
    Resultado resultado;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    resultado = db_abrir(&db, "data/nexaos.db");
    if (resultado != RES_OK) {
        fprintf(stderr, "Não foi possível iniciar o banco de dados.\n");
        return 1;
    }

    printf("NexaOS iniciado - SQLite %s\n", sqlite3_libversion());
    printf("Banco de dados pronto.\n");
    executar_menu_principal(db);
    db_fechar(db);
    return 0;
}
