#ifndef DFX_H
#define DFX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef DFX_VERSION
#define DFX_VERSION "0.2.0"
#endif
#ifdef DFX_ENABLE_LAB_MODE
#define DFX_BUILD_VERSION DFX_VERSION "-lab"
#else
#define DFX_BUILD_VERSION DFX_VERSION
#endif
#define DFX_PATH_MAX 1024
#define DFX_ID_MAX 256
#define DFX_MODEL_MAX 256
#define DFX_DETAIL_MAX 128
#define DFX_ERROR_MAX 512
#define DFX_RESULT_ERROR -1
#define DFX_RESULT_INDETERMINATE -2

typedef enum {
    DFX_MEDIA_UNKNOWN,
    DFX_MEDIA_HDD,
    DFX_MEDIA_SSD,
    DFX_MEDIA_NVME,
    DFX_MEDIA_FLASH,
    DFX_MEDIA_FILE
} dfx_media_kind;

typedef enum {
    DFX_METHOD_AUTO,
    DFX_METHOD_CLEAR_ZERO,
    DFX_METHOD_PURGE_NATIVE,
    DFX_METHOD_DESTROY_PHYSICAL
} dfx_method;

typedef enum {
    DFX_NATIVE_NONE,
    DFX_NATIVE_NVME_BLOCK,
    DFX_NATIVE_NVME_CRYPTO
} dfx_native_action;

typedef enum {
    DFX_NATIVE_STATUS_NONE,
    DFX_NATIVE_STATUS_COMMAND,
    DFX_NATIVE_STATUS_SANITIZE_LOG
} dfx_native_status_source;

typedef struct {
    char path[DFX_PATH_MAX];
    char stable_id[DFX_ID_MAX];
    char model[DFX_MODEL_MAX];
    char firmware[DFX_DETAIL_MAX];
    char transport[DFX_DETAIL_MAX];
    char environment[DFX_DETAIL_MAX];
    char topology[DFX_ID_MAX];
    char qualification_id[DFX_ID_MAX];
    uint64_t size_bytes;
    uint64_t object_identity_a;
    uint64_t object_identity_b;
    uint32_t logical_block_size;
    dfx_media_kind kind;
    bool removable;
    bool internal;
    bool mounted;
    bool in_use;
    bool system_disk;
    bool read_only;
    bool regular_file;
    bool whole_device;
    bool safety_state_known;
    bool stable_identity_unique;
    bool qualified;
} dfx_device;

typedef struct {
    dfx_device device;
    dfx_method method;
    dfx_method requested_method;
    dfx_native_action native_action;
    bool full_verify;
    bool verification_explicit;
    bool execute;
    bool acknowledged;
    bool allow_internal;
    bool lab_mode;
    char confirmation[DFX_ID_MAX];
    char audit_path[DFX_PATH_MAX];
    char operator_id[DFX_ID_MAX];
    char witness_id[DFX_ID_MAX];
    char operation_id[65];
    char final_stable_id[DFX_ID_MAX];
    uint64_t final_size_bytes;
    bool final_identity_observed;
    uint32_t native_status_raw;
    bool native_status_observed;
    dfx_native_status_source native_status_source;
    uint64_t audit_identity_a;
    uint64_t audit_identity_b;
    bool audit_identity_observed;
} dfx_job;

typedef void (*dfx_progress_fn)(uint64_t done, uint64_t total, void *context);

int dfx_list_devices(FILE *out, char error[DFX_ERROR_MAX]);
int dfx_inspect_device(const char *path, dfx_device *device, char error[DFX_ERROR_MAX]);
int dfx_validate_job(const dfx_job *job, char error[DFX_ERROR_MAX]);
int dfx_resolve_method(dfx_job *job, char error[DFX_ERROR_MAX]);
void dfx_apply_qualification(dfx_job *job);
int dfx_run_job(dfx_job *job, dfx_progress_fn progress, void *context, char error[DFX_ERROR_MAX]);
int dfx_native_purge_supported(const dfx_device *device, char error[DFX_ERROR_MAX]);
int dfx_native_purge_action(const dfx_device *device, dfx_native_action *action, char error[DFX_ERROR_MAX]);
int dfx_run_native_purge(dfx_job *job, dfx_progress_fn progress, void *context, char error[DFX_ERROR_MAX]);
void dfx_request_cancel(void);
bool dfx_cancel_requested(void);
bool dfx_lab_mode_available(void);
bool dfx_text_is_safe(const char *text);
bool dfx_hardware_id_is_credible(const char *text);
int dfx_write_audit(dfx_job *job, const char *status, const char *detail, char error[DFX_ERROR_MAX]);
int dfx_verify_audit_file(const char *path, FILE *out, char error[DFX_ERROR_MAX]);
int dfx_prepare_audit(dfx_job *job, char error[DFX_ERROR_MAX]);
const char *dfx_media_name(dfx_media_kind kind);
const char *dfx_method_name(dfx_method method);
const char *dfx_native_action_name(dfx_native_action action);
const char *dfx_native_status_source_name(dfx_native_status_source source);
void dfx_format_size(uint64_t bytes, char *buffer, size_t size);

#endif
