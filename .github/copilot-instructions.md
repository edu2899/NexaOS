# NexaOS — regras fixas do projeto

Projeto acadêmico (PIM IV, UNIP): sistema de ordens de serviço da empresa fictícia NexaFix. Aplicação de terminal em C para Windows. O desenvolvedor é iniciante em C: priorize simplicidade e clareza.

## Tecnologia

- C11, GCC (MSYS2 UCRT64), SQLite 3 pela API C, VS Code, Windows.
- Proibido: C++, Python, Java, JavaScript, web, frameworks e bibliotecas além da biblioteca padrão C e do SQLite (<windows.h> só para configurar o console em UTF-8).
- Não criar nada fora do escopo pedido (login, pagamentos, integrações, exclusão de registros).

## Código

- Módulo = src/<nome>.c + include/<nome>.h (com include guard). main.c só controla o menu principal.
- Arquivos, funções e variáveis em português, snake_case, sem acento. Comentários em português, só onde ajudam. Textos exibidos ao usuário em português, com acento.
- Descrições de teste (TESTE_VERIFICAR) sem acentos.
- Funções curtas, com uma responsabilidade (de preferência menos de 40 linhas). Funções de banco (testáveis, sem printf/scanf) separadas das funções de menu.
- Funções de dados retornam o enum Resultado (include/resultado.h) e devolvem dados por parâmetros de ponteiro.
- Compilar sem warnings com -std=c11 -Wall -Wextra -Wpedantic.
- Proibido: gets, scanf para texto, strcpy, strcat, sprintf. Usar fgets, snprintf e strtol com validação.
- Todo SQL com prepared statements (sqlite3*prepare_v2 + sqlite3_bind*\*); nunca concatenar entrada do usuário no SQL.
- Checar o retorno de toda chamada SQLite, malloc e fopen. Liberar tudo (sqlite3_finalize, free, fclose), inclusive nos caminhos de erro.
- Operações que alteram mais de uma tabela usam transação (BEGIN IMMEDIATE / COMMIT / ROLLBACK).

## Forma de trabalho

- Implementar só a etapa pedida, sem TODO, sem funções incompletas e sem versões apenas demonstrativas.
- Ao terminar: compilar, rodar os testes e informar arquivos criados/alterados, como validar no Windows e, em linguagem simples, o que cada arquivo novo faz.
- Se o pedido for ambíguo ou conflitar com estas regras, perguntar antes de implementar.
