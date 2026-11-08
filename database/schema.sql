-- NexaOS - esquema do banco (SQLite 3). Datas em texto: AAAA-MM-DD HH:MM:SS (horário local).

CREATE TABLE IF NOT EXISTS clientes (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    nome          TEXT NOT NULL,
    documento     TEXT NOT NULL UNIQUE,  -- CPF ou CNPJ, somente dígitos
    telefone      TEXT NOT NULL,
    email         TEXT,
    endereco      TEXT,
    data_cadastro TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS equipamentos (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    cliente_id   INTEGER NOT NULL REFERENCES clientes(id),
    tipo         TEXT NOT NULL CHECK (tipo IN ('COMPUTADOR','NOTEBOOK','IMPRESSORA','OUTRO')),
    marca        TEXT NOT NULL,
    modelo       TEXT NOT NULL,
    numero_serie TEXT NOT NULL UNIQUE
);

CREATE TABLE IF NOT EXISTS tecnicos (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    nome          TEXT NOT NULL UNIQUE,
    especialidade TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS ordens_servico (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    equipamento_id     INTEGER NOT NULL REFERENCES equipamentos(id),
    tecnico_id         INTEGER REFERENCES tecnicos(id),  -- NULL = ainda sem técnico
    descricao_problema TEXT NOT NULL,
    diagnostico        TEXT,
    status             TEXT NOT NULL DEFAULT 'ABERTA' CHECK (status IN
        ('ABERTA','EM_DIAGNOSTICO','AGUARDANDO_APROVACAO','APROVADA',
         'AGUARDANDO_PECA','EM_REPARO','CONCLUIDA','ENTREGUE','CANCELADA')),
    data_abertura      TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS pecas (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    codigo         TEXT NOT NULL UNIQUE,
    nome           TEXT NOT NULL,
    quantidade     INTEGER NOT NULL DEFAULT 0 CHECK (quantidade >= 0),
    estoque_minimo INTEGER NOT NULL DEFAULT 0 CHECK (estoque_minimo >= 0)
);

CREATE TABLE IF NOT EXISTS itens_ordem (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    ordem_id   INTEGER NOT NULL REFERENCES ordens_servico(id),
    peca_id    INTEGER NOT NULL REFERENCES pecas(id),
    quantidade INTEGER NOT NULL CHECK (quantidade > 0),
    data_uso   TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS historico_ordens (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    ordem_id        INTEGER NOT NULL REFERENCES ordens_servico(id),
    tipo_evento     TEXT NOT NULL CHECK (tipo_evento IN
        ('ABERTURA','STATUS','DIAGNOSTICO','OBSERVACAO','TECNICO','PECA')),
    status_anterior TEXT,
    status_novo     TEXT,
    descricao       TEXT,
    data_hora       TEXT NOT NULL DEFAULT (datetime('now','localtime'))
);

CREATE TABLE IF NOT EXISTS movimentacoes_estoque (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    peca_id    INTEGER NOT NULL REFERENCES pecas(id),
    ordem_id   INTEGER REFERENCES ordens_servico(id),  -- preenchido quando a saída é consumo em uma OS
    tipo       TEXT NOT NULL CHECK (tipo IN ('ENTRADA','SAIDA')),
    quantidade INTEGER NOT NULL CHECK (quantidade > 0),
    motivo     TEXT,
    data_hora  TEXT NOT NULL DEFAULT (datetime('now','localtime')),
    CHECK (ordem_id IS NULL OR tipo = 'SAIDA')
);

CREATE INDEX IF NOT EXISTS idx_equipamentos_cliente ON equipamentos(cliente_id);
CREATE INDEX IF NOT EXISTS idx_os_equipamento ON ordens_servico(equipamento_id);
CREATE INDEX IF NOT EXISTS idx_os_status ON ordens_servico(status);
CREATE INDEX IF NOT EXISTS idx_itens_ordem ON itens_ordem(ordem_id);
CREATE INDEX IF NOT EXISTS idx_historico_ordem ON historico_ordens(ordem_id);
CREATE INDEX IF NOT EXISTS idx_mov_peca ON movimentacoes_estoque(peca_id);
