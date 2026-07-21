#include "dfx.h"

#include <string.h>

typedef struct {
    const char *version;
    const char *platform;
    const char *model;
    const char *firmware;
    const char *transport;
    const char *environment;
    const char *topology;
    dfx_media_kind kind;
    dfx_method method;
    dfx_native_action native_action;
    uint64_t size_bytes;
    uint32_t logical_block_size;
    bool full_verify;
    const char *qualification_id;
} dfx_qualification;

static const dfx_qualification qualifications[] = {
#ifdef DFX_TEST_QUALIFICATION
    {"0.2.0", "test", "MODÈLE-QUALIFIÉ", "FW-1", "SATA", "SYSTÈME-TEST", "TOPOLOGIE-TEST", DFX_MEDIA_HDD, DFX_METHOD_CLEAR_ZERO, DFX_NATIVE_NONE, 1000000U, 512U, true, "Q3-TEST-001"},
#endif
    {NULL, NULL, NULL, NULL, NULL, NULL, NULL, DFX_MEDIA_UNKNOWN, DFX_METHOD_AUTO, DFX_NATIVE_NONE, 0, 0, false, NULL}
};

static const char *platform_name(void) {
#ifdef DFX_TEST_QUALIFICATION
    return "test";
#elif defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "inconnu";
#endif
}

void dfx_apply_qualification(dfx_job *job) {
    job->device.qualified = false;
    job->device.qualification_id[0] = '\0';
    if (job->device.regular_file) {
        job->device.qualified = true;
        snprintf(job->device.qualification_id, sizeof(job->device.qualification_id), "VIRTUEL-TEST");
        return;
    }
    const char *platform = platform_name();
    for (size_t index = 0; qualifications[index].qualification_id != NULL; index++) {
        const dfx_qualification *entry = &qualifications[index];
        if (strcmp(entry->version, DFX_BUILD_VERSION) == 0 && strcmp(entry->platform, platform) == 0 && strcmp(entry->model, job->device.model) == 0 && strcmp(entry->firmware, job->device.firmware) == 0 && strcmp(entry->transport, job->device.transport) == 0 && strcmp(entry->environment, job->device.environment) == 0 && strcmp(entry->topology, job->device.topology) == 0 && entry->kind == job->device.kind && entry->method == job->method && entry->native_action == job->native_action && entry->size_bytes == job->device.size_bytes && entry->logical_block_size == job->device.logical_block_size && entry->full_verify == job->full_verify) {
            job->device.qualified = true;
            snprintf(job->device.qualification_id, sizeof(job->device.qualification_id), "%s", entry->qualification_id);
            return;
        }
    }
}
