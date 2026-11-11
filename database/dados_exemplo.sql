-- Dados fictícios para carregar em um banco NexaOS já criado e vazio.
-- As chaves estrangeiras são ativadas antes da transação de carga.
PRAGMA foreign_keys = ON;
BEGIN;

-- Clientes fictícios; o quarto não informa e-mail nem endereço.
INSERT INTO clientes (id, nome, documento, telefone, email, endereco, data_cadastro) VALUES
    (1, 'Ana Lima', '11122233301', '11990001001', 'ana@example.test', 'Rua das Flores, 10', '2026-01-05 09:00:00'),
    (2, 'Bruno Reis', '11122233302', '11990001002', 'bruno@example.test', 'Avenida Central, 20', '2026-01-06 09:30:00'),
    (3, 'Carla Souza', '11122233303', '11990001003', 'carla@example.test', 'Rua do Sol, 30', '2026-01-07 10:00:00'),
    (4, 'Diego Alves', '11122233304', '11990001004', '', '', '2026-01-08 10:30:00');

-- Equipamentos associados a clientes existentes.
INSERT INTO equipamentos (id, cliente_id, tipo, marca, modelo, numero_serie) VALUES
    (1, 1, 'NOTEBOOK', 'Nexa', 'Book 14', 'EXEMPLO-NB-001'),
    (2, 2, 'COMPUTADOR', 'Nexa', 'Desktop 500', 'EXEMPLO-PC-002'),
    (3, 3, 'IMPRESSORA', 'Print', 'Laser 200', 'EXEMPLO-IMP-003'),
    (4, 4, 'NOTEBOOK', 'Nexa', 'Book 15', 'EXEMPLO-NB-004'),
    (5, 1, 'COMPUTADOR', 'Nexa', 'Desktop 300', 'EXEMPLO-PC-005');

-- Técnicos fictícios.
INSERT INTO tecnicos (id, nome, especialidade) VALUES
    (1, 'Rafael Costa', 'Computadores'),
    (2, 'Marina Lopes', 'Impressoras'),
    (3, 'Paulo Mendes', 'Notebooks');

-- Estoque final após as movimentações de saída registradas abaixo.
INSERT INTO pecas (id, codigo, nome, quantidade, estoque_minimo) VALUES
    (1, 'SSD480', 'SSD 480 GB', 18, 5),
    (2, 'BAT15', 'Bateria notebook 15', 7, 3),
    (3, 'TONER12', 'Toner preto', 5, 2),
    (4, 'FONTE500', 'Fonte 500 W', 7, 2),
    (5, 'CABO-HDMI', 'Cabo HDMI', 4, 5),
    (6, 'MEM8', 'Memória 8 GB', 4, 2);

-- Ordens em situações variadas, todas abertas em meses diferentes de 2026.
INSERT INTO ordens_servico
    (id, equipamento_id, tecnico_id, descricao_problema, diagnostico, status, data_abertura)
VALUES
    (1, 1, NULL, 'Notebook não liga.', NULL, 'ABERTA', '2026-01-15 09:00:00'),
    (2, 2, 1, 'Computador reinicia sozinho.', NULL, 'EM_DIAGNOSTICO', '2026-02-10 10:00:00'),
    (3, 3, 2, 'Impressora apresenta falhas na impressão.', 'Rolo de alimentação desgastado.', 'AGUARDANDO_APROVACAO', '2026-03-12 11:00:00'),
    (4, 4, 3, 'Notebook está lento.', 'SSD e bateria precisam ser substituídos.', 'EM_REPARO', '2026-04-14 12:00:00'),
    (5, 5, 1, 'Computador não inicia.', 'Fonte de alimentação com falha.', 'CONCLUIDA', '2026-05-16 13:00:00'),
    (6, 1, 2, 'Notebook não carrega a bateria.', 'Bateria sem retenção de carga.', 'ENTREGUE', '2026-06-18 14:00:00'),
    (7, 2, NULL, 'Solicitação cancelada pelo cliente.', NULL, 'CANCELADA', '2026-07-20 15:00:00'),
    (8, 1, 3, 'Notebook está lento.', 'Memória precisa ser substituída.', 'APROVADA', '2026-08-22 16:00:00');

-- Cada ordem tem abertura; os eventos refletem sua situação e seus dados atuais.
INSERT INTO historico_ordens
    (id, ordem_id, tipo_evento, status_anterior, status_novo, descricao, data_hora)
VALUES
    (1, 1, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-01-15 09:00:00'),
    (2, 2, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-02-10 10:00:00'),
    (3, 2, 'TECNICO', NULL, NULL, 'Técnico atribuído: Rafael Costa', '2026-02-10 10:05:00'),
    (4, 2, 'STATUS', 'ABERTA', 'EM_DIAGNOSTICO', 'Situação atualizada.', '2026-02-10 10:10:00'),
    (5, 3, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-03-12 11:00:00'),
    (6, 3, 'TECNICO', NULL, NULL, 'Técnico atribuído: Marina Lopes', '2026-03-12 11:05:00'),
    (7, 3, 'STATUS', 'ABERTA', 'EM_DIAGNOSTICO', 'Situação atualizada.', '2026-03-12 11:10:00'),
    (8, 3, 'DIAGNOSTICO', NULL, NULL, 'Rolo de alimentação desgastado.', '2026-03-12 11:30:00'),
    (9, 3, 'STATUS', 'EM_DIAGNOSTICO', 'AGUARDANDO_APROVACAO', 'Aguardando aprovação do cliente.', '2026-03-12 11:35:00'),
    (10, 4, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-04-14 12:00:00'),
    (11, 4, 'TECNICO', NULL, NULL, 'Técnico atribuído: Paulo Mendes', '2026-04-14 12:05:00'),
    (12, 4, 'STATUS', 'ABERTA', 'EM_DIAGNOSTICO', 'Situação atualizada.', '2026-04-14 12:10:00'),
    (13, 4, 'DIAGNOSTICO', NULL, NULL, 'SSD e bateria precisam ser substituídos.', '2026-04-14 12:30:00'),
    (14, 4, 'STATUS', 'EM_DIAGNOSTICO', 'AGUARDANDO_APROVACAO', 'Aguardando aprovação do cliente.', '2026-04-14 12:35:00'),
    (15, 4, 'STATUS', 'AGUARDANDO_APROVACAO', 'APROVADA', 'Serviço aprovado.', '2026-04-14 12:40:00'),
    (16, 4, 'STATUS', 'APROVADA', 'EM_REPARO', 'Reparo iniciado.', '2026-04-14 12:45:00'),
    (17, 4, 'PECA', NULL, NULL, 'Peça usada: SSD480 - SSD 480 GB (x2)', '2026-04-14 13:00:00'),
    (18, 4, 'PECA', NULL, NULL, 'Peça usada: BAT15 - Bateria notebook 15 (x1)', '2026-04-14 13:05:00'),
    (19, 5, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-05-16 13:00:00'),
    (20, 5, 'TECNICO', NULL, NULL, 'Técnico atribuído: Rafael Costa', '2026-05-16 13:05:00'),
    (21, 5, 'STATUS', 'ABERTA', 'EM_DIAGNOSTICO', 'Situação atualizada.', '2026-05-16 13:10:00'),
    (22, 5, 'DIAGNOSTICO', NULL, NULL, 'Fonte de alimentação com falha.', '2026-05-16 13:30:00'),
    (23, 5, 'STATUS', 'EM_DIAGNOSTICO', 'AGUARDANDO_APROVACAO', 'Aguardando aprovação do cliente.', '2026-05-16 13:35:00'),
    (24, 5, 'STATUS', 'AGUARDANDO_APROVACAO', 'APROVADA', 'Serviço aprovado.', '2026-05-16 13:40:00'),
    (25, 5, 'STATUS', 'APROVADA', 'EM_REPARO', 'Reparo iniciado.', '2026-05-16 13:45:00'),
    (26, 5, 'PECA', NULL, NULL, 'Peça usada: FONTE500 - Fonte 500 W (x1)', '2026-05-16 14:00:00'),
    (28, 5, 'STATUS', 'EM_REPARO', 'CONCLUIDA', 'Serviço concluído.', '2026-05-16 15:00:00'),
    (29, 6, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-06-18 14:00:00'),
    (30, 6, 'TECNICO', NULL, NULL, 'Técnico atribuído: Marina Lopes', '2026-06-18 14:05:00'),
    (31, 6, 'STATUS', 'ABERTA', 'EM_DIAGNOSTICO', 'Situação atualizada.', '2026-06-18 14:10:00'),
    (32, 6, 'DIAGNOSTICO', NULL, NULL, 'Bateria sem retenção de carga.', '2026-06-18 14:30:00'),
    (33, 6, 'STATUS', 'EM_DIAGNOSTICO', 'AGUARDANDO_APROVACAO', 'Aguardando aprovação do cliente.', '2026-06-18 14:35:00'),
    (34, 6, 'STATUS', 'AGUARDANDO_APROVACAO', 'APROVADA', 'Serviço aprovado.', '2026-06-18 14:40:00'),
    (35, 6, 'STATUS', 'APROVADA', 'EM_REPARO', 'Reparo iniciado.', '2026-06-18 14:45:00'),
    (37, 6, 'PECA', NULL, NULL, 'Peça usada: BAT15 - Bateria notebook 15 (x2)', '2026-06-18 15:05:00'),
    (39, 6, 'STATUS', 'EM_REPARO', 'CONCLUIDA', 'Serviço concluído.', '2026-06-18 16:00:00'),
    (40, 6, 'STATUS', 'CONCLUIDA', 'ENTREGUE', 'Equipamento entregue ao cliente.', '2026-06-18 16:30:00'),
    (41, 7, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-07-20 15:00:00'),
    (42, 7, 'STATUS', 'ABERTA', 'CANCELADA', 'Cancelamento solicitado pelo cliente.', '2026-07-20 15:20:00'),
    (43, 8, 'ABERTURA', NULL, 'ABERTA', 'OS aberta', '2026-08-22 16:00:00'),
    (44, 8, 'TECNICO', NULL, NULL, 'Técnico atribuído: Paulo Mendes', '2026-08-22 16:05:00'),
    (45, 8, 'STATUS', 'ABERTA', 'EM_DIAGNOSTICO', 'Situação atualizada.', '2026-08-22 16:10:00'),
    (46, 8, 'DIAGNOSTICO', NULL, NULL, 'Conjunto de alimentação precisa de substituição.', '2026-08-22 16:30:00'),
    (47, 8, 'STATUS', 'EM_DIAGNOSTICO', 'AGUARDANDO_APROVACAO', 'Aguardando aprovação do cliente.', '2026-08-22 16:35:00'),
    (48, 8, 'STATUS', 'AGUARDANDO_APROVACAO', 'APROVADA', 'Serviço aprovado.', '2026-08-22 16:40:00'),
    (49, 8, 'PECA', NULL, NULL, 'Peça usada: MEM8 - Memória 8 GB (x2)', '2026-08-22 17:00:00');

-- As entradas representam o estoque inicial de cada peça.
INSERT INTO movimentacoes_estoque
    (id, peca_id, ordem_id, tipo, quantidade, motivo, data_hora)
VALUES
    (1, 1, NULL, 'ENTRADA', 20, 'Estoque inicial', '2026-01-02 08:00:00'),
    (2, 2, NULL, 'ENTRADA', 10, 'Estoque inicial', '2026-01-02 08:05:00'),
    (3, 3, NULL, 'ENTRADA', 5, 'Estoque inicial', '2026-01-02 08:10:00'),
    (4, 4, NULL, 'ENTRADA', 8, 'Estoque inicial', '2026-01-02 08:15:00'),
    (5, 5, NULL, 'ENTRADA', 4, 'Estoque inicial', '2026-01-02 08:20:00'),
    (6, 6, NULL, 'ENTRADA', 6, 'Estoque inicial', '2026-01-02 08:25:00'),
    (7, 1, 4, 'SAIDA', 2, 'Uso na OS 4', '2026-04-14 13:00:00'),
    (8, 2, 4, 'SAIDA', 1, 'Uso na OS 4', '2026-04-14 13:05:00'),
    (9, 4, 5, 'SAIDA', 1, 'Uso na OS 5', '2026-05-16 14:00:00'),
    (12, 2, 6, 'SAIDA', 2, 'Uso na OS 6', '2026-06-18 15:05:00'),
    (14, 6, 8, 'SAIDA', 2, 'Uso na OS 8', '2026-08-22 17:00:00');

-- Cada item corresponde a uma saída e a um evento PECA no histórico.
INSERT INTO itens_ordem (id, ordem_id, peca_id, quantidade, data_uso) VALUES
    (1, 4, 1, 2, '2026-04-14 13:00:00'),
    (2, 4, 2, 1, '2026-04-14 13:05:00'),
    (3, 5, 4, 1, '2026-05-16 14:00:00'),
    (6, 6, 2, 2, '2026-06-18 15:05:00'),
    (8, 8, 6, 2, '2026-08-22 17:00:00');

COMMIT;
