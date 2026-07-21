#ifndef DFX_PURGE_INTERNAL_H
#define DFX_PURGE_INTERNAL_H

#include "dfx.h"

#include <stdbool.h>
#include <stdint.h>

#define DFX_NVME_ADMIN_GET_LOG_PAGE 0x02U
#define DFX_NVME_ADMIN_IDENTIFY 0x06U
#define DFX_NVME_ADMIN_SANITIZE 0x84U
#define DFX_NVME_LOG_SANITIZE 0x81U
#define DFX_NVME_SANITIZE_BLOCK 0x02U
#define DFX_NVME_SANITIZE_CRYPTO 0x04U
#define DFX_NVME_IDENTIFY_SIZE 4096U
#define DFX_NVME_SANITIZE_LOG_SIZE 512U
#define DFX_NVME_SANICAP_OFFSET 328U
#define DFX_NVME_NAMESPACE_COUNT_OFFSET 516U
#define DFX_NVME_SANICAP_CRYPTO 0x01U
#define DFX_NVME_SANICAP_BLOCK 0x02U
#define DFX_NVME_SANITIZE_STATUS_MASK 0x07U
#define DFX_NVME_SANITIZE_NEVER 0x00U
#define DFX_NVME_SANITIZE_SUCCESS 0x01U
#define DFX_NVME_SANITIZE_IN_PROGRESS 0x02U
#define DFX_NVME_SANITIZE_FAILED 0x03U
#define DFX_NVME_SANITIZE_SUCCESS_NO_DEALLOCATE 0x04U
#define DFX_NVME_PENDING_LIMIT 15U
#define DFX_NVME_POLL_SECONDS 2U
#define DFX_NVME_MAX_POLL_ROUNDS (7U * 24U * 60U * 60U / DFX_NVME_POLL_SECONDS)

typedef struct {
    void *context;
    int (*open_device)(void *context, const char *path, bool writable, int *system_error);
    int (*admin_command)(void *context, int descriptor, uint8_t opcode, uint32_t namespace_id, void *data, uint32_t data_length, uint32_t cdw10, int *system_error);
    int (*validate_device)(void *context, int descriptor, const dfx_device *device, const unsigned char *identify, size_t identify_size, char error[DFX_ERROR_MAX]);
    void (*close_device)(void *context, int descriptor);
    int (*sleep_seconds)(void *context, unsigned seconds, int *system_error);
} dfx_nvme_ops;

int dfx_nvme_select_action(const unsigned char *identify, size_t identify_size, uint32_t *action, char error[DFX_ERROR_MAX]);
int dfx_nvme_native_purge_supported_with_ops(const dfx_device *device, const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]);
int dfx_nvme_native_purge_action_with_ops(const dfx_device *device, dfx_native_action *action, const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]);
int dfx_nvme_run_native_purge_with_ops(dfx_job *job, dfx_progress_fn progress, void *context, const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]);

#endif
