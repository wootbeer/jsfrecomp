#ifndef JSF_SHA256_H
#define JSF_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct jsf_sha256 {
    uint32_t state[8];
    uint64_t length;
    uint8_t  block[64];
    size_t   used;
} jsf_sha256;

void jsf_sha256_init(jsf_sha256 *ctx);
void jsf_sha256_update(jsf_sha256 *ctx, const void *data, size_t size);
void jsf_sha256_final(jsf_sha256 *ctx, uint8_t out[32]);

/* Lower-case hex of a 32-byte digest into out (65 bytes incl. terminator). */
void jsf_sha256_hex(const uint8_t digest[32], char out[65]);

#endif
