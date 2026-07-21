#include "dfx_sha256.h"

#include <stdio.h>
#include <string.h>

static int verify(const char *input, const char *expected) {
    char actual[65];
    dfx_sha256_hex((const unsigned char *)input, strlen(input), actual);
    if (strcmp(actual, expected) == 0) return 0;
    fprintf(stderr, "SHA-256 incorrect pour « %s » : %s\n", input, actual);
    return 1;
}

int main(void) {
    int failures = 0;
    failures += verify("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    failures += verify("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    failures += verify("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    failures += verify("Les données sont effacées.", "61a76e8678c2786960c3847a91365ae2953602e0e4d1e816773165e1945cee3b");
    return failures == 0 ? 0 : 1;
}
