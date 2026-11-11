# Requisitos atendidos

As referências abaixo apontam para arquivos e funções presentes no projeto.

## Módulos

| Requisito | Onde está no projeto |
|---|---|
| Clientes | [`src/clientes.c`](../src/clientes.c): `cliente_validar`, `cliente_inserir`, `cliente_buscar_por_id`, `cliente_atualizar`, `cliente_listar`, `menu_clientes`. |
| Equipamentos | [`src/equipamentos.c`](../src/equipamentos.c): `equipamento_validar`, `equipamento_inserir`, `equipamento_buscar_por_id`, `equipamento_listar`, `equipamento_listar_por_cliente`, `menu_equipamentos`. |
| Técnicos | [`src/tecnicos.c`](../src/tecnicos.c): `tecnico_validar`, `tecnico_inserir`, `tecnico_buscar_por_id`, `tecnico_listar`, `menu_tecnicos`. |
| Ordens de serviço | [`src/ordens.c`](../src/ordens.c): `ordem_abrir`, `ordem_buscar_por_id`, `ordem_listar`, `ordem_atribuir_tecnico`, `ordem_registrar_diagnostico`, `ordem_registrar_observacao`, `ordem_atualizar_status`. |
| Estoque | [`src/pecas.c`](../src/pecas.c): `peca_cadastrar`, `peca_registrar_entrada`, `peca_registrar_saida`, `estoque_listar_movimentacoes`; tela em [`src/menu_estoque.c`](../src/menu_estoque.c): `menu_estoque`. |
| Peças nas ordens | [`src/itens_ordem.c`](../src/itens_ordem.c): `item_ordem_usar_peca`, `item_ordem_listar`; tela em [`src/menu_itens_ordem.c`](../src/menu_itens_ordem.c): `menu_itens_ordem`. |
| Histórico | [`src/historico.c`](../src/historico.c): `historico_registrar`, `historico_listar_por_ordem`; gravações chamadas por `ordem_abrir`, `ordem_atualizar_status` e `item_ordem_usar_peca`. |
| Relatórios | [`src/relatorios.c`](../src/relatorios.c): `relatorio_resumo_ordens`, `relatorio_ordens_por_mes`, `relatorio_total_mes`, `relatorio_total_status`; tela em [`src/menu_relatorios.c`](../src/menu_relatorios.c): `menu_relatorios`. |
| Exportação CSV | [`src/exportacao.c`](../src/exportacao.c): `exportar_resumo_ordens`, `exportar_ordens_por_mes`, `exportar_estoque_baixo`, `exportar_movimentacoes`; tela em [`src/menu_exportacao.c`](../src/menu_exportacao.c): `menu_exportacao`. |

## Conceitos da linguagem C

| Requisito | Onde está no projeto |
|---|---|
| `if` / `else` e `switch` | `estoque_listar_tela`, em [`src/menu_estoque.c`](../src/menu_estoque.c), usa condições para resultado e lista vazia; `executar_opcao`, em [`src/main.c`](../src/main.c), distribui opções com `switch`. |
| `for` e `while` | `relatorio_calcular_resumo`, em [`src/relatorios.c`](../src/relatorios.c), percorre situações com `for`; `executar_menu_principal`, em [`src/main.c`](../src/main.c), repete o menu com `while`. |
| Funções | Operações divididas entre funções como `ordem_abrir`, `ordem_inserir` e `ordem_registrar_abertura`, em [`src/ordens.c`](../src/ordens.c). |
| `struct` | `Cliente`, `Equipamento`, `Tecnico`, `OrdemServico`, `Peca`, `ItemOrdem` e `ResumoOrdens`, declaradas nos cabeçalhos de [`include/`](../include). |
| Vetores | Listas dinâmicas de registros, por exemplo `Cliente **lista` em `cliente_listar`, declarada em [`include/clientes.h`](../include/clientes.h) e implementada em [`src/clientes.c`](../src/clientes.c). |
| Matriz 12×9 | `int matriz[12][OS_STATUS_TOTAL]` em `relatorio_ordens_por_mes`, em [`src/relatorios.c`](../src/relatorios.c). |
| Matriz 9×9 | `transicoes[OS_STATUS_TOTAL][OS_STATUS_TOTAL]` em [`src/ordens.c`](../src/ordens.c). |
| Ponteiros | Parâmetros de banco, saída e listas, como `sqlite3 *db`, `Cliente *saida` e `Cliente **lista`, em [`include/clientes.h`](../include/clientes.h). |
| Alocação dinâmica | Listagens usam `util_dobrar_capacidade`, em [`src/util.c`](../src/util.c), e liberam seus vetores com `free`. |
| Manipulação de arquivos | [`src/db.c`](../src/db.c): `db_ler_arquivo` lê o esquema; [`src/exportacao.c`](../src/exportacao.c): `csv_abrir` e `csv_fechar` escrevem CSV. |
| Modularização | [`src/main.c`](../src/main.c) chama os menus; os módulos são compilados a partir da variável `MODULOS` em [`build.bat`](../build.bat). |

## Qualidade e persistência

| Requisito | Onde está no projeto |
|---|---|
| Prepared statements | Funções de dados usam `sqlite3_prepare_v2`, vínculos como `sqlite3_bind_int` ou `db_vincular_texto` e finalização com `db_finalizar`; por exemplo, `cliente_inserir` em [`src/clientes.c`](../src/clientes.c). |
| Chaves estrangeiras | `db_ativar_chaves_estrangeiras` em [`src/db.c`](../src/db.c) ativa e confere o pragma; as referências estão em [`database/schema.sql`](../database/schema.sql). |
| Transações | `db_iniciar_transacao`, `db_confirmar` e `db_desfazer` em [`src/db.c`](../src/db.c); são usadas, por exemplo, por `ordem_abrir` e `item_ordem_usar_peca`. |
| Validação de entradas | `util_ler_inteiro`, `util_ler_linha` e validadores de cliente em [`src/util.c`](../src/util.c) e [`src/clientes.c`](../src/clientes.c). |
| Erros de banco | `db_traduzir_erro` em [`src/db.c`](../src/db.c) converte erros SQLite para `Resultado`; as telas exibem `util_texto_resultado`, em [`src/util.c`](../src/util.c). |
| Memória liberada | Listagens liberam vetores em erro e em consumo; [`tests/vazamento.c`](../tests/vazamento.c) rastreia alocações e `teste_codigo_saida`, em [`tests/test_util.h`](../tests/test_util.h), verifica o saldo ao final. |

## Testes

| Requisito | Onde está no projeto |
|---|---|
| Testes automatizados | Os arquivos `tests/test_*.c` são compilados e executados individualmente pelo bloco `test` de [`build.bat`](../build.bat); as verificações usam `TESTE_VERIFICAR`, em [`tests/test_util.h`](../tests/test_util.h). |
| Cobertura dos módulos e validações | [`tests/test_clientes.c`](../tests/test_clientes.c), [`tests/test_db.c`](../tests/test_db.c), [`tests/test_equipamentos.c`](../tests/test_equipamentos.c), [`tests/test_tecnicos.c`](../tests/test_tecnicos.c), [`tests/test_ordens.c`](../tests/test_ordens.c), [`tests/test_ordens_fluxo.c`](../tests/test_ordens_fluxo.c), [`tests/test_pecas.c`](../tests/test_pecas.c), [`tests/test_itens_ordem.c`](../tests/test_itens_ordem.c), [`tests/test_relatorios.c`](../tests/test_relatorios.c), [`tests/test_exportacao.c`](../tests/test_exportacao.c) e [`tests/test_validacao.c`](../tests/test_validacao.c). |
| Dados de exemplo e integridade | [`tests/test_dados_exemplo.c`](../tests/test_dados_exemplo.c) carrega [`database/dados_exemplo.sql`](../database/dados_exemplo.sql) e verifica tabelas, chaves estrangeiras, saldos, ordens, resumo, estoque baixo e matriz mensal. |
