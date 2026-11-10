# Guia do código NexaOS

Este guia acompanha a implementação atual do projeto. Os nomes de arquivo e
função abaixo apontam para o código que executa as operações descritas.

## 1. Visão geral

O programa começa em [`main`](../src/main.c), abre o banco pelo módulo
[`db`](../src/db.c) e mostra o menu principal. Cada opção chama uma tela de
menu; as telas leem os dados do usuário e chamam funções dos módulos de dados.
Esses módulos validam as operações e usam SQLite para consultar ou alterar o
banco. O módulo de banco concentra abertura, esquema, transações e tratamento
de erros.

## 2. Arquivos de código

| Arquivo | Responsabilidade |
|---|---|
| [`src/main.c`](../src/main.c) | Inicializa o banco e direciona o usuário aos menus principais. |
| [`src/db.c`](../src/db.c) | Abre e fecha SQLite, carrega o esquema, executa SQL e controla transações. |
| [`src/util.c`](../src/util.c) | Reúne leitura do teclado, validações, formatação e mensagens compartilhadas. |
| [`src/clientes.c`](../src/clientes.c) | Valida, cadastra, consulta, atualiza e lista clientes; inclui a tela de clientes. |
| [`src/equipamentos.c`](../src/equipamentos.c) | Valida e mantém equipamentos e oferece seu menu. |
| [`src/tecnicos.c`](../src/tecnicos.c) | Valida, cadastra e consulta técnicos e oferece seu menu. |
| [`src/ordens.c`](../src/ordens.c) | Abre e atualiza ordens, valida transições e mantém suas consultas. |
| [`src/historico.c`](../src/historico.c) | Registra e lista eventos do histórico das ordens. |
| [`src/menu_ordens.c`](../src/menu_ordens.c) | Apresenta as telas do fluxo de ordens e encaminha as ações. |
| [`src/pecas.c`](../src/pecas.c) | Mantém peças, quantidades e movimentações de estoque. |
| [`src/menu_estoque.c`](../src/menu_estoque.c) | Apresenta cadastro, consulta e movimentação de estoque. |
| [`src/itens_ordem.c`](../src/itens_ordem.c) | Registra o uso de peças em ordens e lista esses itens. |
| [`src/menu_itens_ordem.c`](../src/menu_itens_ordem.c) | Apresenta as operações de peças usadas em uma ordem. |
| [`src/relatorios.c`](../src/relatorios.c) | Calcula resumos de ordens e totais mensais. |
| [`src/menu_relatorios.c`](../src/menu_relatorios.c) | Apresenta relatórios de ordens e estoque. |
| [`src/exportacao.c`](../src/exportacao.c) | Escreve relatórios e movimentações em arquivos CSV. |
| [`src/menu_exportacao.c`](../src/menu_exportacao.c) | Solicita a exportação e monta o nome do arquivo. |

Os cabeçalhos correspondentes em [`include/`](../include) declaram estruturas,
enumerações e funções compartilhadas entre os módulos.

## 3. Três operações passo a passo

### Cadastrar um cliente

1. A opção de clientes em `executar_opcao` chama `menu_clientes` em
   [`src/clientes.c`](../src/clientes.c).
2. A tela lê os campos com as funções de entrada de `src/util.c` e monta uma
   estrutura `Cliente`.
3. `cliente_inserir` chama `cliente_validar`. Se os campos forem válidos,
   prepara um `INSERT` com parâmetros `?1` a `?5`, vincula cada valor e executa
   a instrução.
4. Ao concluir, obtém o identificador gerado e finaliza a instrução SQLite.
   Um erro de documento duplicado é convertido para `RES_DUPLICADO`.

### Abrir uma ordem de serviço

1. `ordem_abrir`, em [`src/ordens.c`](../src/ordens.c), valida os argumentos e
   confirma que equipamento e técnico (se informado) existem.
2. Inicia uma transação com `db_iniciar_transacao`, que executa
   `BEGIN IMMEDIATE`.
3. `ordem_inserir` grava a ordem com a situação inicial `ABERTA`.
4. `ordem_registrar_abertura` grava o evento `ABERTURA` no histórico e, se
   houver técnico, também registra o evento `TECNICO`.
5. Se as gravações ocorrerem, `db_confirmar` executa `COMMIT`. Se uma delas ou
   o commit falhar, `ordem_reverter` executa `ROLLBACK` e o identificador
   devolvido é zerado.

### Usar uma peça em uma ordem

1. `item_ordem_usar_peca`, em [`src/itens_ordem.c`](../src/itens_ordem.c),
   valida os identificadores e a quantidade, e inicia uma transação antes de
   ler a ordem.
2. Busca a ordem e aceita somente `APROVADA`, `AGUARDANDO_PECA` ou `EM_REPARO`.
3. Dentro da mesma transação, faz quatro gravações: atualiza a quantidade da
   peça; registra a movimentação `SAIDA`; insere a linha em `itens_ordem`; e
   insere o evento `PECA` no histórico. A movimentação inclui o identificador
   da ordem e o motivo `Uso na OS <id>`.
4. Se tudo der certo, confirma a transação. Se qualquer gravação ou o commit
   falhar, desfaz o conjunto inteiro. Assim não fica baixa de estoque sem item
   nem item sem histórico.

## 4. Conceitos de programação no projeto

| Conceito | Onde observar |
|---|---|
| `if` / `else` | Apresentação de erro, lista vazia ou dados em `estoque_listar_tela`, em [`src/menu_estoque.c`](../src/menu_estoque.c). |
| `switch` | Distribuição de opções em `executar_opcao`, em [`src/main.c`](../src/main.c). |
| `for` | Percurso de situações em `relatorio_calcular_resumo`, em [`src/relatorios.c`](../src/relatorios.c). |
| `while` | Leitura repetida das opções em `executar_menu_principal`, em [`src/main.c`](../src/main.c). |
| Funções | Cada módulo divide a tarefa em funções como `ordem_abrir`, `ordem_inserir` e `ordem_registrar_abertura`, em [`src/ordens.c`](../src/ordens.c). |
| `struct` | `Cliente`, `OrdemServico`, `Peca`, `ItemOrdem` e `ResumoOrdens`, declaradas nos cabeçalhos de [`include/`](../include). |
| Vetores | Listas como `Cliente *lista` em `cliente_listar`, em [`src/clientes.c`](../src/clientes.c). |
| Matrizes | `transicoes[OS_STATUS_TOTAL][OS_STATUS_TOTAL]`, uma matriz 9×9 em [`src/ordens.c`](../src/ordens.c); `matriz[12][OS_STATUS_TOTAL]`, uma matriz 12×9 em [`src/relatorios.c`](../src/relatorios.c). |
| Ponteiros | Parâmetros como `sqlite3 *db`, `Cliente *saida` e `Cliente **lista` nas funções de [`include/clientes.h`](../include/clientes.h). |
| Alocação dinâmica | Listagens aumentam vetores com `util_dobrar_capacidade`, em [`src/util.c`](../src/util.c), e liberam a memória com `free`. |
| Arquivos | `csv_abrir` usa `fopen` para escrever CSV em [`src/exportacao.c`](../src/exportacao.c); `db_ler_arquivo` lê `database/schema.sql` em [`src/db.c`](../src/db.c). |
| Modularização | `main.c` chama menus; cada menu encaminha ações ao módulo de dados e inclui o cabeçalho correspondente. |

## 5. Decisões importantes

- **Prepared statements:** consultas usam `sqlite3_prepare_v2`, parâmetros
  vinculados e `sqlite3_finalize`. Dados digitados não são concatenados ao SQL.
- **Transações:** operações que gravam em mais de uma tabela usam
  `BEGIN IMMEDIATE`, `COMMIT` e, em caso de falha, `ROLLBACK`.
- **Códigos de situação sem acento:** o banco guarda identificadores estáveis,
  por exemplo `EM_DIAGNOSTICO`; o programa mostra rótulos acentuados, como
  “Em diagnóstico”.
- **Estoque e movimentações juntos:** cada entrada ou saída atualiza
  `pecas.quantidade` e registra uma linha em `movimentacoes_estoque` na mesma
  transação. O histórico permite conferir como a quantidade foi formada.
- **Enum `Resultado`:** as funções devolvem um código como `RES_OK`,
  `RES_INVALIDO` ou `RES_NAO_ENCONTRADO`. A tela converte o código para uma
  mensagem por `util_texto_resultado`.

## 6. Perguntas que podem aparecer na apresentação

1. **Por que usar SQLite?** É um banco local em arquivo, suficiente para este
   sistema acadêmico e acessível pela API C.
2. **O que é um prepared statement?** É uma instrução SQL preparada uma vez,
   cujos valores são vinculados separadamente antes da execução.
3. **Por que a abertura de ordem usa transação?** Para gravar a ordem e seus
   eventos de histórico como uma única operação.
4. **O que acontece se uma etapa da transação falhar?** O programa executa
   `ROLLBACK`, cancelando as gravações daquela operação.
5. **Por que há códigos e rótulos de situação separados?** O código salvo no
   banco é estável e sem acento; o rótulo é apropriado para exibição.
6. **Como o programa evita estoque negativo?** A atualização da saída só
   subtrai a quantidade se o estoque disponível for suficiente.
7. **Por que a movimentação é registrada junto com a quantidade?** Para manter
   o saldo atual e seu histórico consistentes.
8. **O que significa `Cliente **lista`?** A função pode devolver ao chamador o
   endereço de um vetor de clientes alocado dinamicamente.
9. **Como uma lista vazia é representada?** A função devolve `RES_OK`,
   quantidade zero e ponteiro `NULL`.
10. **Como o programa informa erros às telas?** As funções de dados devolvem
    `Resultado`; a tela obtém a mensagem com `util_texto_resultado`.

## 7. Limitações conhecidas

- Não há operação para excluir registros.
- Cancelar uma ordem não devolve automaticamente as peças já consumidas.
- CPF e CNPJ são validados pelo formato e pela quantidade de dígitos, mas não
  pelo cálculo do dígito verificador.
- O sistema não tem login, pagamentos ou integração com serviços externos.
- Os relatórios e exportações refletem os dados locais existentes no banco;
  não há sincronização entre computadores.
