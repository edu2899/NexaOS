#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int util_eh_espaco_ponta(char caractere)
{
    return caractere == ' ' || caractere == '\t' || caractere == '\n' ||
           caractere == '\r';
}

void util_trim(char *texto)
{
    char *inicio;
    size_t tamanho;

    if (texto == NULL) {
        return;
    }

    inicio = texto;
    while (*inicio != '\0' && util_eh_espaco_ponta(*inicio)) {
        inicio++;
    }

    tamanho = strlen(inicio);
    while (tamanho > 0 && util_eh_espaco_ponta(inicio[tamanho - 1])) {
        tamanho--;
    }

    if (inicio != texto) {
        memmove(texto, inicio, tamanho);
    }
    texto[tamanho] = '\0';
}

Resultado util_ler_linha(const char *mensagem, char *destino, size_t tamanho)
{
    size_t comprimento;
    int caractere;

    if (destino == NULL || tamanho < 2 || tamanho > INT_MAX) {
        return RES_INVALIDO;
    }

    if (mensagem != NULL) {
        fputs(mensagem, stdout);
        fflush(stdout);
    }

    if (fgets(destino, (int)tamanho, stdin) == NULL) {
        return RES_ERRO_ARQUIVO;
    }

    comprimento = strlen(destino);
    if (comprimento > 0 && destino[comprimento - 1] == '\n') {
        destino[--comprimento] = '\0';
    } else if (comprimento == tamanho - 1) {
        caractere = fgetc(stdin);
        if (caractere != '\n' && caractere != EOF) {
            do {
                caractere = fgetc(stdin);
            } while (caractere != '\n' && caractere != EOF);
        }
    }

    util_trim(destino);
    return RES_OK;
}

Resultado util_ler_texto_obrigatorio(const char *mensagem, char *destino,
                                     size_t tamanho)
{
    Resultado resultado;

    do {
        resultado = util_ler_linha(mensagem, destino, tamanho);
        if (resultado != RES_OK) {
            return resultado;
        }
        if (destino[0] == '\0') {
            util_erro("O texto não pode ficar vazio.");
        }
    } while (destino[0] == '\0');

    return RES_OK;
}

Resultado util_ler_inteiro(const char *mensagem, int minimo, int maximo,
                           int *valor)
{
    char texto[128];
    char *fim;
    long numero;
    Resultado resultado;

    if (valor == NULL || minimo > maximo) {
        return RES_INVALIDO;
    }

    for (;;) {
        resultado = util_ler_linha(mensagem, texto, sizeof(texto));
        if (resultado != RES_OK) {
            return resultado;
        }

        errno = 0;
        fim = NULL;
        numero = strtol(texto, &fim, 10);
        if (texto[0] != '\0' && errno != ERANGE && fim != texto &&
            *fim == '\0' && numero >= INT_MIN && numero <= INT_MAX &&
            numero >= minimo && numero <= maximo) {
            *valor = (int)numero;
            return RES_OK;
        }

        util_erro("Digite um número inteiro dentro do intervalo permitido.");
    }
}

Resultado util_ler_confirmacao(const char *mensagem, int *confirmado)
{
    char texto[32];
    Resultado resultado;

    if (confirmado == NULL) {
        return RES_INVALIDO;
    }

    for (;;) {
        resultado = util_ler_linha(mensagem, texto, sizeof(texto));
        if (resultado != RES_OK) {
            return resultado;
        }

        if (texto[1] == '\0' &&
            (texto[0] == 'S' || texto[0] == 's')) {
            *confirmado = 1;
            return RES_OK;
        }
        if (texto[1] == '\0' &&
            (texto[0] == 'N' || texto[0] == 'n')) {
            *confirmado = 0;
            return RES_OK;
        }

        util_erro("Responda S para sim ou N para não.");
    }
}

void util_somente_digitos(const char *origem, char *destino, size_t tamanho)
{
    size_t indice_origem = 0;
    size_t indice_destino = 0;

    if (destino == NULL || tamanho == 0) {
        return;
    }
    destino[0] = '\0';
    if (origem == NULL) {
        return;
    }

    while (origem[indice_origem] != '\0' && indice_destino + 1 < tamanho) {
        if (isdigit((unsigned char)origem[indice_origem])) {
            destino[indice_destino++] = origem[indice_origem];
        }
        indice_origem++;
    }
    destino[indice_destino] = '\0';
}

static int util_texto_tem_apenas_digitos(const char *texto)
{
    size_t i;

    if (texto == NULL || texto[0] == '\0') {
        return 0;
    }

    for (i = 0; texto[i] != '\0'; i++) {
        if (!isdigit((unsigned char)texto[i])) {
            return 0;
        }
    }
    return 1;
}

int util_validar_documento(const char *digitos)
{
    size_t tamanho;

    if (!util_texto_tem_apenas_digitos(digitos)) {
        return 0;
    }

    tamanho = strlen(digitos);
    return tamanho == 11 || tamanho == 14;
}

int util_validar_telefone(const char *digitos)
{
    size_t tamanho;

    if (!util_texto_tem_apenas_digitos(digitos)) {
        return 0;
    }

    tamanho = strlen(digitos);
    return tamanho == 10 || tamanho == 11;
}

int util_validar_email(const char *email)
{
    const char *arroba = NULL;
    const char *ponto = NULL;
    const char *caractere;

    if (email == NULL) {
        return 0;
    }

    for (caractere = email; *caractere != '\0'; caractere++) {
        if (isspace((unsigned char)*caractere)) {
            return 0;
        }
        if (*caractere == '@') {
            if (arroba != NULL) {
                return 0;
            }
            arroba = caractere;
        }
    }

    if (arroba == NULL || arroba == email || arroba[1] == '\0') {
        return 0;
    }

    for (caractere = arroba + 1; *caractere != '\0'; caractere++) {
        if (*caractere == '.') {
            ponto = caractere;
            break;
        }
    }

    return ponto != NULL && ponto != arroba + 1 && ponto[1] != '\0';
}

void *util_dobrar_capacidade(void *vetor, size_t *capacidade,
                             size_t tamanho_item)
{
    size_t nova_capacidade;
    void *temporario;

    if (capacidade == NULL || *capacidade == 0 || tamanho_item == 0 ||
        *capacidade > (size_t)-1 / 2) {
        return NULL;
    }
    nova_capacidade = *capacidade * 2;
    if (nova_capacidade > (size_t)-1 / tamanho_item) {
        return NULL;
    }

    temporario = realloc(vetor, nova_capacidade * tamanho_item);
    if (temporario == NULL) {
        return NULL;
    }
    *capacidade = nova_capacidade;
    return temporario;
}

void util_imprimir_coluna(const char *texto, int largura)
{
    const unsigned char *inicio = (const unsigned char *)
        (texto != NULL ? texto : "");
    const unsigned char *cursor = inicio;
    int caracteres = 0;

    while (*cursor != '\0' && caracteres < largura) {
        cursor++;
        while ((*cursor & 0xC0) == 0x80) {
            cursor++;
        }
        caracteres++;
    }
    if (caracteres > 0) {
        fwrite(inicio, 1, (size_t)(cursor - inicio), stdout);
    }
    while (caracteres < largura) {
        putchar(' ');
        caracteres++;
    }
    putchar(' ');
}

void util_formatar_data(const char *iso, char *destino, size_t tamanho)
{
    size_t i;
    int formato_valido = iso != NULL && strlen(iso) == 19 &&
        iso[4] == '-' && iso[7] == '-' && iso[10] == ' ' &&
        iso[13] == ':' && iso[16] == ':';

    if (destino == NULL || tamanho == 0) {
        return;
    }
    if (formato_valido) {
        for (i = 0; i < 19; i++) {
            if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 &&
                (iso[i] < '0' || iso[i] > '9')) {
                formato_valido = 0;
                break;
            }
        }
    }
    if (formato_valido) {
        snprintf(destino, tamanho, "%.2s/%.2s/%.4s%s", iso + 8, iso + 5,
                 iso, iso + 10);
    } else {
        snprintf(destino, tamanho, "%s", iso != NULL ? iso : "");
    }
}

void util_titulo(const char *titulo)
{
    printf("========== %s ==========\n", titulo != NULL ? titulo : "");
}

void util_sucesso(const char *mensagem)
{
    printf("[OK] %s\n", mensagem != NULL ? mensagem : "");
}

void util_erro(const char *mensagem)
{
    printf("[ERRO] %s\n", mensagem != NULL ? mensagem : "");
}

void util_pausar(void)
{
    char linha[8];

    (void)util_ler_linha("Pressione Enter para continuar...", linha,
                         sizeof(linha));
}

const char *util_texto_resultado(Resultado resultado)
{
    switch (resultado) {
        case RES_OK:
            return "Operação concluída com sucesso.";
        case RES_ERRO_BANCO:
            return "Ocorreu um erro no banco de dados.";
        case RES_DUPLICADO:
            return "O registro já existe.";
        case RES_NAO_ENCONTRADO:
            return "Registro não encontrado.";
        case RES_INVALIDO:
            return "Dado inválido.";
        case RES_ERRO_MEMORIA:
            return "Não há memória suficiente.";
        case RES_ERRO_ARQUIVO:
            return "Não foi possível ler ou acessar o arquivo.";
        case RES_REGRA_NEGOCIO:
            return "Operação não permitida pelas regras do sistema (verifique situação, técnico e diagnóstico da ordem).";
        case RES_ESTOQUE_INSUFICIENTE:
            return "Estoque insuficiente para esta saída.";
        default:
            return "Resultado desconhecido.";
    }
}
