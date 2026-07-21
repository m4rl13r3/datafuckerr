#define _POSIX_C_SOURCE 200809L

#include "dfx.h"
#include "purge_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(__linux__)

#include <fcntl.h>
#include <linux/fs.h>
#include <linux/nvme_ioctl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#endif

static uint16_t read_le16(const unsigned char *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t read_le32(const unsigned char *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool nvme_path_is_direct_namespace(const char *path) {
    if (path == NULL || strncmp(path, "/dev/nvme", 9) != 0) return false;
    const char *cursor = path + 9;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') cursor++;
    if (*cursor != 'n') return false;
    cursor++;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') cursor++;
    return *cursor == '\0';
}

static int validate_ops(const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]) {
    if (ops == NULL || ops->open_device == NULL || ops->admin_command == NULL || ops->validate_device == NULL || ops->close_device == NULL || ops->sleep_seconds == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Le backend NVMe interne est incomplet.");
        return -1;
    }
    return 0;
}

static int nvme_command(const dfx_nvme_ops *ops, int descriptor, uint8_t opcode, uint32_t namespace_id, void *data, uint32_t data_length, uint32_t cdw10, const char *operation, char error[DFX_ERROR_MAX]) {
    int system_error = 0;
    int result = ops->admin_command(ops->context, descriptor, opcode, namespace_id, data, data_length, cdw10, &system_error);
    if (result < 0) {
        if (system_error == 0) system_error = EIO;
        snprintf(error, DFX_ERROR_MAX, "%s impossible : %s", operation, strerror(system_error));
        return -1;
    }
    if (result != 0) {
        snprintf(error, DFX_ERROR_MAX, "%s refusée par le contrôleur NVMe, statut 0x%x.", operation, result);
        return -1;
    }
    return 0;
}

int dfx_nvme_select_action(const unsigned char *identify, size_t identify_size, uint32_t *action, char error[DFX_ERROR_MAX]) {
    if (identify == NULL || identify_size < DFX_NVME_NAMESPACE_COUNT_OFFSET + sizeof(uint32_t) || action == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Les données d’identification NVMe sont invalides.");
        return -1;
    }
    uint32_t capabilities = read_le32(identify + DFX_NVME_SANICAP_OFFSET);
    uint32_t namespace_count = read_le32(identify + DFX_NVME_NAMESPACE_COUNT_OFFSET);
    if (namespace_count != 1U) {
        snprintf(error, DFX_ERROR_MAX, "Refus : Sanitize vise tout le sous-système NVMe et ce contrôleur annonce %u espaces de noms.", namespace_count);
        return -1;
    }
    if ((capabilities & DFX_NVME_SANICAP_CRYPTO) != 0U) {
        *action = DFX_NVME_SANITIZE_CRYPTO;
        return 0;
    }
    if ((capabilities & DFX_NVME_SANICAP_BLOCK) != 0U) {
        *action = DFX_NVME_SANITIZE_BLOCK;
        return 0;
    }
    snprintf(error, DFX_ERROR_MAX, "Le contrôleur NVMe n’expose ni crypto-erase ni block-erase Sanitize.");
    return -1;
}

static int identify_controller(const dfx_nvme_ops *ops, int descriptor, unsigned char identify[DFX_NVME_IDENTIFY_SIZE], char error[DFX_ERROR_MAX]) {
    memset(identify, 0, DFX_NVME_IDENTIFY_SIZE);
    return nvme_command(ops, descriptor, DFX_NVME_ADMIN_IDENTIFY, 0U, identify, DFX_NVME_IDENTIFY_SIZE, 1U, "Identification du contrôleur", error);
}

static int select_action(const dfx_nvme_ops *ops, int descriptor, uint32_t *action, unsigned char output[DFX_NVME_IDENTIFY_SIZE], char error[DFX_ERROR_MAX]) {
    unsigned char local[DFX_NVME_IDENTIFY_SIZE];
    unsigned char *identify = output == NULL ? local : output;
    if (identify_controller(ops, descriptor, identify, error) != 0) return -1;
    return dfx_nvme_select_action(identify, DFX_NVME_IDENTIFY_SIZE, action, error);
}

static int open_nvme(const dfx_device *device, bool writable, const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]) {
    if (validate_ops(ops, error) != 0) return -1;
    if (device == NULL || device->kind != DFX_MEDIA_NVME || !nvme_path_is_direct_namespace(device->path)) {
        snprintf(error, DFX_ERROR_MAX, "La purge native intégrée est actuellement limitée aux périphériques NVMe Linux directs.");
        return -1;
    }
    int system_error = 0;
    int descriptor = ops->open_device(ops->context, device->path, writable, &system_error);
    if (descriptor < 0) {
        if (system_error == 0) system_error = EIO;
        snprintf(error, DFX_ERROR_MAX, "Ouverture NVMe impossible : %s", strerror(system_error));
    }
    return descriptor;
}

int dfx_nvme_native_purge_supported_with_ops(const dfx_device *device, const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]) {
    int descriptor = open_nvme(device, false, ops, error);
    if (descriptor < 0) return -1;
    uint32_t action = 0;
    int result = select_action(ops, descriptor, &action, NULL, error);
    ops->close_device(ops->context, descriptor);
    return result;
}

int dfx_nvme_native_purge_action_with_ops(const dfx_device *device, dfx_native_action *action, const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]) {
    if (action == NULL) {
        snprintf(error, DFX_ERROR_MAX, "La sous-méthode NVMe est absente.");
        return -1;
    }
    int descriptor = open_nvme(device, false, ops, error);
    if (descriptor < 0) return -1;
    uint32_t command_action = 0;
    int result = select_action(ops, descriptor, &command_action, NULL, error);
    ops->close_device(ops->context, descriptor);
    if (result != 0) return -1;
    *action = command_action == DFX_NVME_SANITIZE_CRYPTO ? DFX_NATIVE_NVME_CRYPTO : DFX_NATIVE_NVME_BLOCK;
    return 0;
}

static int sanitize_log(const dfx_nvme_ops *ops, int descriptor, uint16_t *progress, uint16_t *status, char error[DFX_ERROR_MAX]) {
    unsigned char log[DFX_NVME_SANITIZE_LOG_SIZE];
    memset(log, 0, sizeof(log));
    uint32_t dwords = (uint32_t)sizeof(log) / 4U;
    uint32_t cdw10 = ((dwords - 1U) << 16) | DFX_NVME_LOG_SANITIZE;
    if (nvme_command(ops, descriptor, DFX_NVME_ADMIN_GET_LOG_PAGE, UINT32_MAX, log, (uint32_t)sizeof(log), cdw10, "Lecture du journal Sanitize", error) != 0) return -1;
    *progress = read_le16(log);
    *status = read_le16(log + 2);
    return 0;
}

static int wait_before_poll(const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]) {
    int system_error = 0;
    if (ops->sleep_seconds(ops->context, DFX_NVME_POLL_SECONDS, &system_error) == 0) return 0;
    if (system_error == 0) system_error = EIO;
    snprintf(error, DFX_ERROR_MAX, "La purge a démarré mais son attente de suivi a échoué : %s", strerror(system_error));
    return -1;
}

static int send_sanitize(dfx_job *job, const dfx_nvme_ops *ops, int descriptor, uint32_t action, char error[DFX_ERROR_MAX]) {
    int system_error = 0;
    int result = ops->admin_command(ops->context, descriptor, DFX_NVME_ADMIN_SANITIZE, 0U, NULL, 0U, action, &system_error);
    if (result < 0) {
        if (system_error == 0) system_error = EIO;
        snprintf(error, DFX_ERROR_MAX, "L’envoi de Sanitize a échoué sans permettre d’exclure son acceptation par le contrôleur : %s", strerror(system_error));
        return DFX_RESULT_INDETERMINATE;
    }
    if (result != 0) {
        job->native_status_raw = (uint32_t)result;
        job->native_status_observed = true;
        job->native_status_source = DFX_NATIVE_STATUS_COMMAND;
        snprintf(error, DFX_ERROR_MAX, "Commande Sanitize refusée par le contrôleur NVMe, statut 0x%x.", result);
        return -1;
    }
    return 0;
}

int dfx_nvme_run_native_purge_with_ops(dfx_job *job, dfx_progress_fn progress, void *context, const dfx_nvme_ops *ops, char error[DFX_ERROR_MAX]) {
    if (job == NULL) {
        snprintf(error, DFX_ERROR_MAX, "La tâche de purge NVMe est invalide.");
        return -1;
    }
    job->native_status_raw = 0;
    job->native_status_observed = false;
    job->native_status_source = DFX_NATIVE_STATUS_NONE;
    int descriptor = open_nvme(&job->device, true, ops, error);
    if (descriptor < 0) return -1;
    uint32_t action = 0;
    unsigned char identify[DFX_NVME_IDENTIFY_SIZE];
    if (select_action(ops, descriptor, &action, identify, error) != 0) {
        ops->close_device(ops->context, descriptor);
        return -1;
    }
    dfx_native_action selected_action = action == DFX_NVME_SANITIZE_CRYPTO ? DFX_NATIVE_NVME_CRYPTO : DFX_NATIVE_NVME_BLOCK;
    if (selected_action != job->native_action) {
        snprintf(error, DFX_ERROR_MAX, "Refus : la sous-méthode NVMe a changé depuis la planification.");
        ops->close_device(ops->context, descriptor);
        return -1;
    }
    if (ops->validate_device(ops->context, descriptor, &job->device, identify, sizeof(identify), error) != 0) {
        ops->close_device(ops->context, descriptor);
        return -1;
    }
    if (dfx_cancel_requested()) {
        snprintf(error, DFX_ERROR_MAX, "Purge annulée avant l’envoi de la commande Sanitize.");
        ops->close_device(ops->context, descriptor);
        return -1;
    }
    int sanitize_result = send_sanitize(job, ops, descriptor, action, error);
    if (sanitize_result != 0) {
        ops->close_device(ops->context, descriptor);
        return sanitize_result;
    }
    unsigned pending_rounds = 0;
    unsigned poll_rounds = 0;
    bool observed_in_progress = false;
    for (;;) {
        if (dfx_cancel_requested()) {
            snprintf(error, DFX_ERROR_MAX, "Le suivi a été interrompu, mais la purge Sanitize continue dans le contrôleur NVMe ; consultez impérativement son journal Sanitize.");
            ops->close_device(ops->context, descriptor);
            return DFX_RESULT_INDETERMINATE;
        }
        uint16_t completed = 0;
        uint16_t status = 0;
        if (sanitize_log(ops, descriptor, &completed, &status, error) != 0) {
            char detail[DFX_ERROR_MAX];
            snprintf(detail, sizeof(detail), "La purge a démarré mais son suivi a échoué : %.400s", error);
            snprintf(error, DFX_ERROR_MAX, "%s", detail);
            ops->close_device(ops->context, descriptor);
            return DFX_RESULT_INDETERMINATE;
        }
        unsigned state = status & DFX_NVME_SANITIZE_STATUS_MASK;
        job->native_status_raw = status;
        job->native_status_observed = true;
        job->native_status_source = DFX_NATIVE_STATUS_SANITIZE_LOG;
        if (progress != NULL) progress(completed, UINT16_MAX, context);
        if (state == DFX_NVME_SANITIZE_IN_PROGRESS) observed_in_progress = true;
        if ((state == DFX_NVME_SANITIZE_SUCCESS || state == DFX_NVME_SANITIZE_SUCCESS_NO_DEALLOCATE) && observed_in_progress) {
            ops->close_device(ops->context, descriptor);
            return 0;
        }
        if (state == DFX_NVME_SANITIZE_FAILED && observed_in_progress) {
            snprintf(error, DFX_ERROR_MAX, "Le contrôleur NVMe signale l’échec de la purge.");
            ops->close_device(ops->context, descriptor);
            return -1;
        }
        bool waiting_for_current_operation = !observed_in_progress && (state == DFX_NVME_SANITIZE_NEVER || state == DFX_NVME_SANITIZE_SUCCESS || state == DFX_NVME_SANITIZE_SUCCESS_NO_DEALLOCATE || state == DFX_NVME_SANITIZE_FAILED);
        if (waiting_for_current_operation && ++pending_rounds >= DFX_NVME_PENDING_LIMIT) {
            snprintf(error, DFX_ERROR_MAX, "La commande a été acceptée, mais le contrôleur ne permet pas de distinguer son démarrage d’un ancien état Sanitize ; le résultat est indéterminé.");
            ops->close_device(ops->context, descriptor);
            return DFX_RESULT_INDETERMINATE;
        }
        if (!waiting_for_current_operation && state != DFX_NVME_SANITIZE_IN_PROGRESS) {
            snprintf(error, DFX_ERROR_MAX, "État Sanitize NVMe inattendu : %u.", state);
            ops->close_device(ops->context, descriptor);
            return DFX_RESULT_INDETERMINATE;
        }
        if (++poll_rounds >= DFX_NVME_MAX_POLL_ROUNDS) {
            snprintf(error, DFX_ERROR_MAX, "La commande a été acceptée, mais le délai maximal de suivi Sanitize de sept jours est dépassé ; le résultat est indéterminé.");
            ops->close_device(ops->context, descriptor);
            return DFX_RESULT_INDETERMINATE;
        }
        if (wait_before_poll(ops, error) != 0) {
            ops->close_device(ops->context, descriptor);
            return DFX_RESULT_INDETERMINATE;
        }
    }
}

#if defined(__linux__)

static int linux_open_device(void *context, const char *path, bool writable, int *system_error) {
    (void)context;
    int flags = (writable ? O_RDWR : O_RDONLY) | O_CLOEXEC | O_NOFOLLOW;
    if (writable) flags |= O_EXCL;
    int descriptor = open(path, flags);
    if (descriptor < 0) *system_error = errno;
    return descriptor;
}

static int linux_admin_command(void *context, int descriptor, uint8_t opcode, uint32_t namespace_id, void *data, uint32_t data_length, uint32_t cdw10, int *system_error) {
    (void)context;
    struct nvme_admin_cmd command;
    memset(&command, 0, sizeof(command));
    command.opcode = opcode;
    command.nsid = namespace_id;
    command.addr = (uint64_t)(uintptr_t)data;
    command.data_len = data_length;
    command.cdw10 = cdw10;
    int result = ioctl(descriptor, NVME_IOCTL_ADMIN_CMD, &command);
    if (result < 0) *system_error = errno;
    return result;
}

static int nvme_identify_text(const unsigned char *identify, size_t identify_size, size_t offset, size_t length, char *output, size_t output_size) {
    if (identify == NULL || offset > identify_size || length > identify_size - offset || output_size == 0) return -1;
    while (length > 0 && (identify[offset + length - 1] == ' ' || identify[offset + length - 1] == '\0')) length--;
    while (length > 0 && identify[offset] == ' ') {
        offset++;
        length--;
    }
    if (length == 0 || length >= output_size) return -1;
    for (size_t index = 0; index < length; index++) {
        if (identify[offset + index] < 0x20U || identify[offset + index] > 0x7eU) return -1;
    }
    memcpy(output, identify + offset, length);
    output[length] = '\0';
    return 0;
}

static int linux_validate_device(void *context, int descriptor, const dfx_device *device, const unsigned char *identify, size_t identify_size, char error[DFX_ERROR_MAX]) {
    (void)context;
    struct stat status;
    uint64_t size_bytes = 0;
    uint32_t block_size = 0;
    if (fstat(descriptor, &status) != 0 || !S_ISBLK(status.st_mode) || ioctl(descriptor, BLKGETSIZE64, &size_bytes) != 0 || ioctl(descriptor, BLKSSZGET, &block_size) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Validation du descripteur NVMe impossible : %s", strerror(errno));
        return -1;
    }
    if ((uint64_t)status.st_rdev != device->object_identity_a || size_bytes != device->size_bytes || block_size != device->logical_block_size) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le descripteur NVMe ne correspond pas à l’identité ou à la géométrie confirmée.");
        return -1;
    }
    char serial[21];
    char model[41];
    char firmware[9];
    if (nvme_identify_text(identify, identify_size, 4U, 20U, serial, sizeof(serial)) != 0 || !dfx_hardware_id_is_credible(serial) || nvme_identify_text(identify, identify_size, 24U, 40U, model, sizeof(model)) != 0 || nvme_identify_text(identify, identify_size, 64U, 8U, firmware, sizeof(firmware)) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Refus : l’identité NVMe liée au descripteur est absente ou invalide.");
        return -1;
    }
    char stable_id[DFX_ID_MAX];
    snprintf(stable_id, sizeof(stable_id), "linux-%.200s-%llu", serial, (unsigned long long)size_bytes);
    if (strcmp(stable_id, device->stable_id) != 0 || (strcmp(device->model, "Non déterminé") != 0 && strcmp(model, device->model) != 0) || (strcmp(device->firmware, "Non déterminé") != 0 && strcmp(firmware, device->firmware) != 0)) {
        snprintf(error, DFX_ERROR_MAX, "Refus : l’identité NVMe du descripteur a changé depuis la confirmation.");
        return -1;
    }
    return 0;
}

static void linux_close_device(void *context, int descriptor) {
    (void)context;
    close(descriptor);
}

static int linux_sleep_seconds(void *context, unsigned seconds, int *system_error) {
    (void)context;
    struct timespec remaining = {(time_t)seconds, 0};
    while (nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) {
            *system_error = errno;
            return -1;
        }
    }
    return 0;
}

static const dfx_nvme_ops linux_ops = {
    NULL,
    linux_open_device,
    linux_admin_command,
    linux_validate_device,
    linux_close_device,
    linux_sleep_seconds
};

int dfx_native_purge_supported(const dfx_device *device, char error[DFX_ERROR_MAX]) {
    return dfx_nvme_native_purge_supported_with_ops(device, &linux_ops, error);
}

int dfx_native_purge_action(const dfx_device *device, dfx_native_action *action, char error[DFX_ERROR_MAX]) {
    return dfx_nvme_native_purge_action_with_ops(device, action, &linux_ops, error);
}

int dfx_run_native_purge(dfx_job *job, dfx_progress_fn progress, void *context, char error[DFX_ERROR_MAX]) {
    return dfx_nvme_run_native_purge_with_ops(job, progress, context, &linux_ops, error);
}

#else

int dfx_native_purge_supported(const dfx_device *device, char error[DFX_ERROR_MAX]) {
    (void)device;
    snprintf(error, DFX_ERROR_MAX, "La purge native intégrée n’est pas encore disponible sur ce système.");
    return -1;
}

int dfx_native_purge_action(const dfx_device *device, dfx_native_action *action, char error[DFX_ERROR_MAX]) {
    (void)device;
    (void)action;
    snprintf(error, DFX_ERROR_MAX, "La purge native intégrée n’est pas encore disponible sur ce système.");
    return -1;
}

int dfx_run_native_purge(dfx_job *job, dfx_progress_fn progress, void *context, char error[DFX_ERROR_MAX]) {
    (void)job;
    (void)progress;
    (void)context;
    snprintf(error, DFX_ERROR_MAX, "La purge native intégrée n’est pas encore disponible sur ce système.");
    return -1;
}

#endif
