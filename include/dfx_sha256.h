#ifndef DFX_SHA256_H
#define DFX_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_length;
    unsigned char block[64];
    size_t block_length;
} dfx_sha256_context;

void dfx_sha256_init(dfx_sha256_context *context);
void dfx_sha256_update(dfx_sha256_context *context, const unsigned char *data, size_t size);
void dfx_sha256_final(dfx_sha256_context *context, unsigned char output[32]);
void dfx_sha256_final_hex(dfx_sha256_context *context, char output[65]);
void dfx_sha256(const unsigned char *data, size_t size, unsigned char output[32]);
void dfx_sha256_hex(const unsigned char *data, size_t size, char output[65]);

#endif
