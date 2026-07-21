#include "audit_internal.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    dfx_audit_record_syntax_valid(data, size);
    return 0;
}
