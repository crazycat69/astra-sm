/*
 * RC4
 * Copyright (c) 1996-2000 Whistle Communications, Inc.
 */

#include <astra.h>
#include <utils/rc4.h>

static inline
void swap_bytes(uint8_t *a, uint8_t *b)
{
    uint8_t temp;
    temp = *a;
    *a = *b;
    *b = temp;
}

void au_rc4_init(rc4_ctx_t *state, const uint8_t *key, int keylen)
{
    uint8_t j;
    int i;

    for(i = 0; i < 256; i++)
        state->perm[i] = (uint8_t)i;

    state->index1 = 0;
    state->index2 = 0;

    for(j = i = 0; i < 256; i++)
    {
        j += state->perm[i] + key[i % keylen];
        swap_bytes(&state->perm[i], &state->perm[j]);
    }
}

void au_rc4_crypt(rc4_ctx_t *state, uint8_t *dst, const uint8_t *buf
                  , int buflen)
{
    int i;
    uint8_t j;

    for (i = 0; i < buflen; i++)
    {
        state->index1++;
        state->index2 += state->perm[state->index1];

        swap_bytes(&state->perm[state->index1], &state->perm[state->index2]);

        j = state->perm[state->index1] + state->perm[state->index2];
        dst[i] = buf[i] ^ state->perm[j];
    }
}
