#ifndef ORDENS_H
#define ORDENS_H

#include "db.h"

typedef enum StatusOS {
    OS_ABERTA = 0,
    OS_EM_DIAGNOSTICO,
    OS_AGUARDANDO_APROVACAO,
    OS_APROVADA,
    OS_AGUARDANDO_PECA,
    OS_EM_REPARO,
    OS_CONCLUIDA,
    OS_ENTREGUE,
    OS_CANCELADA,
    OS_STATUS_TOTAL
} StatusOS;

typedef struct OrdemServico {
    int id;
    int equipamento_id;
    int cliente_id;
    char cliente_nome[101];
    char equip_tipo[12];
    char equip_marca[61];
    char equip_modelo[61];
    char equip_serie[61];
    int tecnico_id;
    char tecnico_nome[101];
    char descricao_problema[501];
    char diagnostico[501];
    char status[24];
    char data_abertura[20];
} OrdemServico;

const char *os_status_codigo(StatusOS status);
const char *os_status_rotulo(StatusOS status);
int os_status_de_codigo(const char *codigo);
Resultado ordem_abrir(sqlite3 *db, int equipamento_id, int tecnico_id,
                      const char *descricao_problema, int *id_novo);
Resultado ordem_buscar_por_id(sqlite3 *db, int id, OrdemServico *saida);
Resultado ordem_listar(sqlite3 *db, const char *status_codigo,
                       OrdemServico **lista, int *quantidade);
void menu_ordens(sqlite3 *db);

#endif
