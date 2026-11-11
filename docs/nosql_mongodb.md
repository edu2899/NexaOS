# NoSQL e MongoDB

## Relacional e documental

O NexaOS usa SQLite, um banco relacional. Os dados ficam em tabelas com
colunas definidas, como `clientes`, `ordens_servico` e `pecas`. Chaves
estrangeiras relacionam os registros; consultas podem combinar tabelas com
`JOIN`; e transações ajudam a manter gravações relacionadas consistentes.

MongoDB é um banco documental. Um documento JSON pode guardar dados aninhados,
como uma ordem com uma lista de eventos. Documentos da mesma coleção podem ter
campos diferentes, o que torna o formato flexível, mas exige que a aplicação
valide os dados e cuide de relações que não sejam mantidas pelo esquema
relacional.

## Quando usar cada um

- Um banco relacional costuma ser adequado quando há relações claras entre
  entidades, regras de integridade e operações que atualizam várias tabelas
  juntas.
- Um banco documental pode ser adequado quando os dados são naturalmente
  agrupados em documentos, os campos variam entre registros e as leituras
  normalmente recuperam um documento inteiro.
- A escolha depende das consultas, das regras de consistência, da escala e da
  experiência da equipe; nenhum dos formatos é sempre melhor.

## Uma possibilidade para o histórico de ordens

Uma evolução poderia guardar no MongoDB um documento por ordem, com os dados de
identificação da OS e seus eventos aninhados, como testes, medições, observações
e anexos. Um exemplo ilustrativo está em
[`exemplo_historico_ordem.json`](./exemplo_historico_ordem.json).

Nesse desenho, clientes, equipamentos, peças e estoque continuariam no SQLite.
O documento poderia referenciar os identificadores relacionais da ordem e das
entidades relacionadas. A aplicação teria de definir como conferir esses
vínculos e como lidar com falhas entre os dois bancos, pois uma transação do
SQLite não inclui automaticamente uma gravação no MongoDB.

## Vantagens e custos

**Possíveis vantagens do formato documental**

- Eventos com campos diferentes podem ser representados sem adicionar novas
  colunas para cada tipo de detalhe.
- A ordem e seu histórico podem ser lidos juntos em um único documento.

**Pontos de atenção**

- Flexibilidade não substitui validação; documentos diferentes podem acabar
  com formatos inconsistentes.
- Dados repetidos e relações externas precisam de regras de atualização e
  conferência.
- Manter SQLite e MongoDB juntos acrescentaria instalação, configuração,
  operação e tratamento de falhas distribuídas.

## Situação no NexaOS

O MongoDB **não foi integrado ao NexaOS**. O sistema atual usa SQLite para
cadastros, ordens, histórico, estoque, relatórios e exportações. A migração do
histórico é apenas uma possibilidade de evolução e deve depender da confirmação
do tutor e dos requisitos do projeto.
