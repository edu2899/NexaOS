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

/* Devolve o código usado no banco para uma situação. */
const char *os_status_codigo(StatusOS status);
/* Devolve o rótulo exibido para uma situação. */
const char *os_status_rotulo(StatusOS status);
/* Localiza o índice de uma situação pelo código do banco. */
int os_status_de_codigo(const char *codigo);
/* Verifica se uma mudança de situação é permitida. */
int ordem_transicao_permitida(StatusOS atual, StatusOS novo_status);
/* Informa se a ordem está em uma situação encerrada. */
int ordem_encerrada(const char *status_codigo);
/* Abre uma ordem e registra seu evento inicial. */
Resultado ordem_abrir(sqlite3 *db, int equipamento_id, int tecnico_id,
                      const char *descricao_problema, int *id_novo);
/* Busca uma ordem pelo identificador. */
Resultado ordem_buscar_por_id(sqlite3 *db, int id, OrdemServico *saida);
/* Lista ordens, opcionalmente filtradas por situação. */
Resultado ordem_listar(sqlite3 *db, const char *status_codigo,
                       OrdemServico **lista, int *quantidade);
/* Atribui um técnico à ordem. */
Resultado ordem_atribuir_tecnico(sqlite3 *db, int ordem_id, int tecnico_id);
/* Registra o diagnóstico de uma ordem. */
Resultado ordem_registrar_diagnostico(sqlite3 *db, int ordem_id,
                                      const char *diagnostico);
/* Registra uma observação no histórico da ordem. */
Resultado ordem_registrar_observacao(sqlite3 *db, int ordem_id,
                                     const char *observacao);
/* Atualiza a situação da ordem. */
Resultado ordem_atualizar_status(sqlite3 *db, int ordem_id,
                                 StatusOS novo_status,
                                 const char *observacao);

#endif
