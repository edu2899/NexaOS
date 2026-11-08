#include "db.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

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
        fprintf(stderr, "Nao foi possivel iniciar o banco de dados.\n");
        return 1;
    }

    printf("NexaOS iniciado - SQLite %s\n", sqlite3_libversion());
    printf("Banco de dados pronto.\n");
    db_fechar(db);
    return 0;
}
