#include "dfx_sha256.h"

#include <stdint.h>
#include <string.h>

static const uint32_t constants[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

static uint32_t rotate_right(uint32_t value, unsigned count) {
    return (value >> count) | (value << (32U - count));
}

static void transform(dfx_sha256_context *context) {
    uint32_t words[64];
    for (size_t index = 0; index < 16; index++) {
        size_t offset = index * 4;
        words[index] = ((uint32_t)context->block[offset] << 24) | ((uint32_t)context->block[offset + 1] << 16) | ((uint32_t)context->block[offset + 2] << 8) | context->block[offset + 3];
    }
    for (size_t index = 16; index < 64; index++) {
        uint32_t first = rotate_right(words[index - 15], 7) ^ rotate_right(words[index - 15], 18) ^ (words[index - 15] >> 3);
        uint32_t second = rotate_right(words[index - 2], 17) ^ rotate_right(words[index - 2], 19) ^ (words[index - 2] >> 10);
        words[index] = words[index - 16] + first + words[index - 7] + second;
    }
    uint32_t a = context->state[0];
    uint32_t b = context->state[1];
    uint32_t c = context->state[2];
    uint32_t d = context->state[3];
    uint32_t e = context->state[4];
    uint32_t f = context->state[5];
    uint32_t g = context->state[6];
    uint32_t h = context->state[7];
    for (size_t index = 0; index < 64; index++) {
        uint32_t sum_one = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t temporary_one = h + sum_one + choice + constants[index] + words[index];
        uint32_t sum_zero = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temporary_two = sum_zero + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary_one;
        d = c;
        c = b;
        b = a;
        a = temporary_one + temporary_two;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
    context->state[5] += f;
    context->state[6] += g;
    context->state[7] += h;
}

void dfx_sha256_init(dfx_sha256_context *context) {
    context->state[0] = 0x6a09e667U;
    context->state[1] = 0xbb67ae85U;
    context->state[2] = 0x3c6ef372U;
    context->state[3] = 0xa54ff53aU;
    context->state[4] = 0x510e527fU;
    context->state[5] = 0x9b05688cU;
    context->state[6] = 0x1f83d9abU;
    context->state[7] = 0x5be0cd19U;
    context->bit_length = 0;
    context->block_length = 0;
}

void dfx_sha256_update(dfx_sha256_context *context, const unsigned char *data, size_t size) {
    for (size_t index = 0; index < size; index++) {
        context->block[context->block_length++] = data[index];
        if (context->block_length == 64) {
            transform(context);
            context->bit_length += 512;
            context->block_length = 0;
        }
    }
}

void dfx_sha256_final(dfx_sha256_context *context, unsigned char output[32]) {
    size_t index = context->block_length;
    context->block[index++] = 0x80;
    if (index > 56) {
        while (index < 64) context->block[index++] = 0;
        transform(context);
        index = 0;
    }
    while (index < 56) context->block[index++] = 0;
    context->bit_length += (uint64_t)context->block_length * 8U;
    for (size_t byte = 0; byte < 8; byte++) context->block[63 - byte] = (unsigned char)(context->bit_length >> (byte * 8));
    transform(context);
    for (size_t word = 0; word < 8; word++) {
        output[word * 4] = (unsigned char)(context->state[word] >> 24);
        output[word * 4 + 1] = (unsigned char)(context->state[word] >> 16);
        output[word * 4 + 2] = (unsigned char)(context->state[word] >> 8);
        output[word * 4 + 3] = (unsigned char)context->state[word];
    }
}

void dfx_sha256(const unsigned char *data, size_t size, unsigned char output[32]) {
    dfx_sha256_context context;
    dfx_sha256_init(&context);
    dfx_sha256_update(&context, data, size);
    dfx_sha256_final(&context, output);
}

static void digest_hex(const unsigned char digest[32], char output[65]) {
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < 32; index++) {
        output[index * 2] = digits[digest[index] >> 4];
        output[index * 2 + 1] = digits[digest[index] & 0x0fU];
    }
    output[64] = '\0';
}

void dfx_sha256_final_hex(dfx_sha256_context *context, char output[65]) {
    unsigned char digest[32];
    dfx_sha256_final(context, digest);
    digest_hex(digest, output);
}

void dfx_sha256_hex(const unsigned char *data, size_t size, char output[65]) {
    unsigned char digest[32];
    dfx_sha256(data, size, digest);
    digest_hex(digest, output);
}
