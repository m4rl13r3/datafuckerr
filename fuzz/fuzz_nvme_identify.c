#include "purge_internal.h"

#include <stddef.h>
#include <stdint.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint32_t action = 0;
    char error[DFX_ERROR_MAX] = {0};
    dfx_nvme_select_action(data, size, &action, error);
    return 0;
}
