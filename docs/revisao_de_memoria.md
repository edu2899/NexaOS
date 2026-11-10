# Revisão de memória e recursos

Revisão dos recursos adquiridos em `src/`. Os vetores devolvidos por funções de
listagem pertencem ao chamador e devem ser liberados com `free` depois do uso.
As consultas SQLite são finalizadas mesmo quando uma leitura, vinculação ou
execução falha.

| Arquivo | Função | Recurso | Onde é liberado |
|---|---|---|---|
| `src/db.c` | `db_ler_arquivo` | `fopen`, `malloc` | `fclose` em todos os caminhos; `free` se a leitura ou o fechamento falhar; em sucesso, o buffer é transferido a `db_executar_script`. |
| `src/db.c` | `db_executar_script` | buffer de `malloc` | `free` depois de `db_executar_sql`, mesmo quando a execução falha. |
| `src/db.c` | `db_ativar_chaves_estrangeiras` | `sqlite3_prepare_v2` | `sqlite3_finalize` após a leitura e também quando a verificação do pragma falha. |
| `src/db.c` | `db_finalizar` | `sqlite3_finalize` | Finaliza a instrução recebida e traduz falha de finalização quando não havia erro anterior. |
| `src/db.c` | `db_executar_sql` | mensagem de `sqlite3_exec` | `sqlite3_free` tanto no sucesso como no erro. |
| `src/db.c` | `db_abrir`, `db_fechar` | conexão SQLite | `db_fechar` ao falhar a abertura/configuração/esquema e ao encerrar o programa. |
| `src/clientes.c` | `cliente_inserir`, `cliente_buscar_por_id`, `cliente_atualizar`, `cliente_listar` | `sqlite3_prepare_v2` | `db_finalizar` depois da execução; falha de preparação não deixa uma instrução ativa. |
| `src/clientes.c` | `cliente_listar` e auxiliar de leitura | `malloc`, `realloc` via `util_dobrar_capacidade` | `free` após erro ou lista vazia; lista não vazia é entregue ao chamador, que a libera após o uso. |
| `src/equipamentos.c` | `equipamento_inserir`, `equipamento_buscar_por_id`, auxiliares de listagem | `sqlite3_prepare_v2` | `db_finalizar` ao terminar ou ao falhar a execução/vinculação. |
| `src/equipamentos.c` | `equipamento_listar_interno` e auxiliares | `malloc`, `realloc` via `util_dobrar_capacidade` | `free` após erro ou resultado vazio; a tela libera a lista recebida depois de exibi-la. |
| `src/tecnicos.c` | `tecnico_inserir`, `tecnico_buscar_por_id`, `tecnico_listar` | `sqlite3_prepare_v2` | `db_finalizar` depois da execução; falhas de preparação não deixam instrução ativa. |
| `src/tecnicos.c` | `tecnico_listar` e auxiliares | `malloc`, `realloc` via `util_dobrar_capacidade` | `free` após erro ou resultado vazio; a tela libera o vetor após exibi-lo. |
| `src/historico.c` | `historico_registrar`, `historico_listar_por_ordem`, auxiliares | `sqlite3_prepare_v2` | `db_finalizar` em sucesso e erro, inclusive quando a vinculação falha. |
| `src/historico.c` | `historico_listar_por_ordem` e auxiliar de leitura | `malloc`, `realloc` via `util_dobrar_capacidade` | `free` após erro ou resultado vazio; o chamador libera a lista não vazia. |
| `src/ordens.c` | `ordem_abrir`, `ordem_buscar_por_id`, `ordem_listar`, atribuição, diagnóstico, observação e situação | `sqlite3_prepare_v2` | Cada instrução ativa é encerrada por `db_finalizar` após a operação ou falha. |
| `src/ordens.c` | `ordem_listar` e auxiliares | `malloc`, `realloc` via `util_dobrar_capacidade` | `free` após erro ou lista vazia; menus liberam listas não vazias após a exibição. |
| `src/pecas.c` | buscas, cadastro, entrada/saída e preparação de movimentações | `sqlite3_prepare_v2` | `db_finalizar` após uso e em erros de execução/vinculação; a busca de atualização finaliza explicitamente. |
| `src/pecas.c` | `peca_listar_com_filtro`, `estoque_listar_movimentacoes` e auxiliares | `malloc`, `realloc` via `util_dobrar_capacidade` | `free` após erro ou lista vazia; menus e exportação liberam os vetores não vazios. |
| `src/itens_ordem.c` | `item_ordem_usar_peca`, `item_ordem_listar` e auxiliares | `sqlite3_prepare_v2` | `db_finalizar` após a consulta de itens; operações de escrita finalizam suas instruções antes do commit/rollback. |
| `src/itens_ordem.c` | `item_ordem_listar` e auxiliar | `malloc`, `realloc` via `util_dobrar_capacidade` | `free` após erro ou lista vazia; a tela libera a lista após exibi-la. |
| `src/relatorios.c` | `relatorio_resumo_ordens`, `relatorio_ordens_por_mes` | `sqlite3_prepare_v2` | `db_finalizar` em sucesso e erro de leitura ou vinculação. |
| `src/util.c` | `util_dobrar_capacidade` | `realloc` | Em falha, preserva o ponteiro recebido para o chamador liberá-lo; em sucesso, devolve o novo ponteiro ao chamador. |
| `src/exportacao.c` | `csv_abrir`, `csv_fechar` | `fopen` | `fclose` em todas as saídas; arquivo parcial é removido se houver falha de escrita ou fechamento. |
| `src/exportacao.c` | `exportar_estoque_baixo`, `exportar_movimentacoes` | vetores das funções de listagem | `free` se a abertura do CSV falhar e depois da tentativa de escrita. |
| `src/menu_ordens.c` | telas de seleção, listagem e histórico | vetores de equipamentos, técnicos, ordens e eventos | `free` em cada caminho, inclusive erro e lista vazia. |
| `src/menu_estoque.c` | telas de listagem e movimentações | vetores de peças e movimentos | `free` depois de apresentar os dados, inclusive quando a consulta falha. |
| `src/menu_itens_ordem.c` | `item_listar_tela` | vetor de itens | `free` após exibição, erro ou resultado vazio. |
| `src/menu_relatorios.c` | telas de estoque baixo e movimentações | vetores de peças e movimentos | `free` depois de apresentar os dados, inclusive em erro e lista vazia. |
| `src/clientes.c`, `src/equipamentos.c`, `src/tecnicos.c` | telas de listagem | vetores de registros | As próprias telas liberam os vetores retornados pelas funções de listagem. |

## Resultado da auditoria

- Não foi encontrado caminho sem `free`, `fclose` ou `sqlite3_finalize` nos
  recursos auditados.
- `vazamento_iniciar`, chamado no começo de cada teste, registra o saldo de
  alocações anterior ao `main` para não atribuir ao projeto blocos do runtime C.
- `db_executar_script` foi separado da leitura do arquivo: o buffer fica sob
  responsabilidade de `db_ler_arquivo` até ser entregue e é liberado após a
  execução; os caminhos de leitura e fechamento com erro também o liberam.
- A leitura de equipamentos repetia a cópia da mesma linha; a chamada
  redundante foi removida sem alterar os dados produzidos.
- A lista de equipamentos continua sendo liberada pelo chamador após o uso; a
  consulta só preenche cada posição uma vez.

Para verificar alocações e liberações durante os testes, a Parte 3 adiciona
instrumentação ao linker e uma verificação de blocos ainda vivos ao final de
cada executável de teste.
