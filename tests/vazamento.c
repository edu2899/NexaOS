#include <stddef.h>

void *__real_malloc(size_t tamanho);
void *__real_calloc(size_t quantidade, size_t tamanho);
void *__real_realloc(void *ponteiro, size_t tamanho);
void __real_free(void *ponteiro);

static long blocos_vivos = 0;
static long blocos_iniciais = 0;

void *__wrap_malloc(size_t tamanho)
{
    void *ponteiro = __real_malloc(tamanho);

    if (ponteiro != NULL) blocos_vivos++;
    return ponteiro;
}

void *__wrap_calloc(size_t quantidade, size_t tamanho)
{
    void *ponteiro = __real_calloc(quantidade, tamanho);

    if (ponteiro != NULL) blocos_vivos++;
    return ponteiro;
}

void *__wrap_realloc(void *ponteiro, size_t tamanho)
{
    void *novo_ponteiro = __real_realloc(ponteiro, tamanho);

    if (ponteiro == NULL && novo_ponteiro != NULL) blocos_vivos++;
    if (ponteiro != NULL && tamanho == 0 && novo_ponteiro == NULL) {
        blocos_vivos--;
    }
    return novo_ponteiro;
}

void __wrap_free(void *ponteiro)
{
    if (ponteiro != NULL) blocos_vivos--;
    __real_free(ponteiro);
}

long vazamento_pendentes(void)
{
    return blocos_vivos - blocos_iniciais;
}

void vazamento_iniciar(void)
{
    blocos_iniciais = blocos_vivos;
}
