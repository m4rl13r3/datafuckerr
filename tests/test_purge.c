#include "dfx.h"
#include "purge_internal.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MOCK_LOG_MAX 32U

typedef struct {
    unsigned char identify[DFX_NVME_IDENTIFY_SIZE];
    uint16_t log_progress[MOCK_LOG_MAX];
    uint16_t log_status[MOCK_LOG_MAX];
    size_t log_count;
    size_t log_index;
    unsigned open_calls;
    unsigned close_calls;
    unsigned sleep_calls;
    unsigned identify_calls;
    unsigned sanitize_calls;
    unsigned validation_calls;
    unsigned protocol_errors;
    bool opened_writable;
    uint32_t sanitize_action;
    int admin_failure_opcode;
    int admin_failure_result;
    int admin_system_error;
    int sleep_result;
    int sleep_system_error;
    bool cancel_on_sanitize;
    bool repeat_last_log;
    int validation_result;
} mock_nvme;

typedef struct {
    unsigned calls;
    uint64_t last_done;
    uint64_t last_total;
} progress_capture;

static unsigned failures = 0;

static void expect_true(bool condition, const char *name) {
    if (condition) return;
    fprintf(stderr, "ÉCHEC : %s\n", name);
    failures++;
}

static void write_le16(unsigned char *data, uint16_t value) {
    data[0] = (unsigned char)(value & 0xffU);
    data[1] = (unsigned char)((value >> 8) & 0xffU);
}

static void write_le32(unsigned char *data, uint32_t value) {
    data[0] = (unsigned char)(value & 0xffU);
    data[1] = (unsigned char)((value >> 8) & 0xffU);
    data[2] = (unsigned char)((value >> 16) & 0xffU);
    data[3] = (unsigned char)((value >> 24) & 0xffU);
}

static void configure_identify(mock_nvme *mock, uint32_t capabilities, uint32_t namespace_count) {
    memset(mock->identify, 0, sizeof(mock->identify));
    write_le32(mock->identify + DFX_NVME_SANICAP_OFFSET, capabilities);
    write_le32(mock->identify + DFX_NVME_NAMESPACE_COUNT_OFFSET, namespace_count);
}

static int mock_open_device(void *context, const char *path, bool writable, int *system_error) {
    mock_nvme *mock = context;
    mock->open_calls++;
    mock->opened_writable = writable;
    if (strcmp(path, "/dev/nvme0n1") != 0) mock->protocol_errors++;
    *system_error = 0;
    return 41;
}

static int mock_admin_command(void *context, int descriptor, uint8_t opcode, uint32_t namespace_id, void *data, uint32_t data_length, uint32_t cdw10, int *system_error) {
    mock_nvme *mock = context;
    if (descriptor != 41) mock->protocol_errors++;
    if ((int)opcode == mock->admin_failure_opcode) {
        *system_error = mock->admin_system_error;
        return mock->admin_failure_result;
    }
    if (opcode == DFX_NVME_ADMIN_IDENTIFY) {
        mock->identify_calls++;
        if (namespace_id != 0U || data == NULL || data_length != DFX_NVME_IDENTIFY_SIZE || cdw10 != 1U) mock->protocol_errors++;
        if (data != NULL && data_length == DFX_NVME_IDENTIFY_SIZE) memcpy(data, mock->identify, DFX_NVME_IDENTIFY_SIZE);
        return 0;
    }
    if (opcode == DFX_NVME_ADMIN_SANITIZE) {
        mock->sanitize_calls++;
        mock->sanitize_action = cdw10;
        if (namespace_id != 0U || data != NULL || data_length != 0U) mock->protocol_errors++;
        if (mock->cancel_on_sanitize) dfx_request_cancel();
        return 0;
    }
    if (opcode == DFX_NVME_ADMIN_GET_LOG_PAGE) {
        if (namespace_id != UINT32_MAX || data == NULL || data_length != DFX_NVME_SANITIZE_LOG_SIZE || (cdw10 & 0xffU) != DFX_NVME_LOG_SANITIZE) {
            mock->protocol_errors++;
            *system_error = EINVAL;
            return -1;
        }
        if (mock->log_index >= mock->log_count && (!mock->repeat_last_log || mock->log_count == 0)) {
            *system_error = EIO;
            return -1;
        }
        size_t status_index = mock->log_index < mock->log_count ? mock->log_index : mock->log_count - 1U;
        unsigned char *log = data;
        memset(log, 0, data_length);
        write_le16(log, mock->log_progress[status_index]);
        write_le16(log + 2, mock->log_status[status_index]);
        mock->log_index++;
        return 0;
    }
    mock->protocol_errors++;
    *system_error = EINVAL;
    return -1;
}

static int mock_validate_device(void *context, int descriptor, const dfx_device *device, const unsigned char *identify, size_t identify_size, char error[DFX_ERROR_MAX]) {
    mock_nvme *mock = context;
    mock->validation_calls++;
    if (descriptor != 41 || device == NULL || identify == NULL || identify_size != DFX_NVME_IDENTIFY_SIZE || memcmp(identify, mock->identify, identify_size) != 0) mock->protocol_errors++;
    if (mock->validation_result != 0) {
        snprintf(error, DFX_ERROR_MAX, "Identité NVMe injectée différente.");
        return -1;
    }
    return 0;
}

static void mock_close_device(void *context, int descriptor) {
    mock_nvme *mock = context;
    mock->close_calls++;
    if (descriptor != 41) mock->protocol_errors++;
}

static int mock_sleep_seconds(void *context, unsigned seconds, int *system_error) {
    mock_nvme *mock = context;
    mock->sleep_calls++;
    if (seconds != DFX_NVME_POLL_SECONDS) mock->protocol_errors++;
    *system_error = mock->sleep_system_error;
    return mock->sleep_result;
}

static dfx_nvme_ops make_ops(mock_nvme *mock) {
    dfx_nvme_ops ops;
    ops.context = mock;
    ops.open_device = mock_open_device;
    ops.admin_command = mock_admin_command;
    ops.validate_device = mock_validate_device;
    ops.close_device = mock_close_device;
    ops.sleep_seconds = mock_sleep_seconds;
    return ops;
}

static dfx_job make_job(void) {
    dfx_job job;
    memset(&job, 0, sizeof(job));
    snprintf(job.device.path, sizeof(job.device.path), "/dev/nvme0n1");
    job.device.kind = DFX_MEDIA_NVME;
    job.method = DFX_METHOD_PURGE_NATIVE;
    job.native_action = DFX_NATIVE_NVME_CRYPTO;
    return job;
}

static void capture_progress(uint64_t done, uint64_t total, void *context) {
    progress_capture *capture = context;
    capture->calls++;
    capture->last_done = done;
    capture->last_total = total;
}

static void test_action_selection(void) {
    unsigned char identify[DFX_NVME_IDENTIFY_SIZE] = {0};
    char error[DFX_ERROR_MAX] = {0};
    uint32_t action = 0;
    write_le32(identify + DFX_NVME_NAMESPACE_COUNT_OFFSET, 1U);

    expect_true(dfx_nvme_select_action(identify, sizeof(identify), &action, error) != 0, "SANICAP vide refusé");
    expect_true(strstr(error, "ni crypto-erase ni block-erase") != NULL, "diagnostic SANICAP vide");

    write_le32(identify + DFX_NVME_SANICAP_OFFSET, DFX_NVME_SANICAP_BLOCK);
    expect_true(dfx_nvme_select_action(identify, sizeof(identify), &action, error) == 0, "block-erase accepté");
    expect_true(action == DFX_NVME_SANITIZE_BLOCK, "sélection block-erase");

    write_le32(identify + DFX_NVME_SANICAP_OFFSET, DFX_NVME_SANICAP_CRYPTO);
    expect_true(dfx_nvme_select_action(identify, sizeof(identify), &action, error) == 0, "crypto-erase accepté");
    expect_true(action == DFX_NVME_SANITIZE_CRYPTO, "sélection crypto-erase");

    write_le32(identify + DFX_NVME_SANICAP_OFFSET, DFX_NVME_SANICAP_CRYPTO | DFX_NVME_SANICAP_BLOCK);
    expect_true(dfx_nvme_select_action(identify, sizeof(identify), &action, error) == 0, "capacités combinées acceptées");
    expect_true(action == DFX_NVME_SANITIZE_CRYPTO, "priorité au crypto-erase");

    write_le32(identify + DFX_NVME_NAMESPACE_COUNT_OFFSET, 2U);
    expect_true(dfx_nvme_select_action(identify, sizeof(identify), &action, error) != 0, "plusieurs espaces de noms refusés");
    expect_true(strstr(error, "2 espaces de noms") != NULL, "diagnostic du nombre d’espaces de noms");

    write_le32(identify + DFX_NVME_NAMESPACE_COUNT_OFFSET, 0U);
    expect_true(dfx_nvme_select_action(identify, sizeof(identify), &action, error) != 0, "nombre nul d’espaces de noms refusé");

    expect_true(dfx_nvme_select_action(identify, DFX_NVME_SANICAP_OFFSET, &action, error) != 0, "bloc Identify tronqué refusé");
    expect_true(strstr(error, "invalides") != NULL, "diagnostic de bloc Identify tronqué");
}

static void test_support_with_injected_ops(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_native_purge_supported_with_ops(&job.device, &ops, error) == 0, "détection injectée prise en charge");
    expect_true(mock.open_calls == 1U, "ouverture unique pendant la détection");
    expect_true(!mock.opened_writable, "détection ouverte en lecture seule");
    expect_true(mock.identify_calls == 1U, "Identify unique pendant la détection");
    expect_true(mock.close_calls == 1U, "fermeture après détection");
    expect_true(mock.protocol_errors == 0U, "protocole valide pendant la détection");

    dfx_native_action action = DFX_NATIVE_NONE;
    expect_true(dfx_nvme_native_purge_action_with_ops(&job.device, &action, &ops, error) == 0, "sous-méthode injectée détectée");
    expect_true(action == DFX_NATIVE_NVME_CRYPTO, "crypto-erase exposé dans le plan");

    snprintf(job.device.path, sizeof(job.device.path), "/dev/nvme0n1p1");
    expect_true(dfx_nvme_native_purge_supported_with_ops(&job.device, &ops, error) != 0, "partition NVMe refusée");
    expect_true(mock.open_calls == 2U, "partition refusée avant ouverture supplémentaire");
}

static void test_progress_then_success(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO | DFX_NVME_SANICAP_BLOCK, 1U);
    mock.log_count = 3U;
    mock.log_progress[0] = 1000U;
    mock.log_progress[1] = 40000U;
    mock.log_progress[2] = UINT16_MAX;
    mock.log_status[0] = DFX_NVME_SANITIZE_IN_PROGRESS;
    mock.log_status[1] = (uint16_t)(0x100U | DFX_NVME_SANITIZE_IN_PROGRESS);
    mock.log_status[2] = DFX_NVME_SANITIZE_SUCCESS;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    progress_capture capture = {0};
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, capture_progress, &capture, &ops, error) == 0, "progression suivie jusqu’au succès");
    expect_true(mock.opened_writable, "purge ouverte en écriture");
    expect_true(mock.sanitize_calls == 1U, "commande Sanitize unique");
    expect_true(mock.validation_calls == 1U, "identité du descripteur validée avant la commande");
    expect_true(mock.sanitize_action == DFX_NVME_SANITIZE_CRYPTO, "crypto-erase émis au contrôleur");
    expect_true(mock.log_index == 3U, "trois journaux Sanitize lus");
    expect_true(mock.sleep_calls == 2U, "temporisations entre états en cours");
    expect_true(mock.close_calls == 1U, "fermeture après succès");
    expect_true(mock.protocol_errors == 0U, "protocole valide pendant le succès");
    expect_true(capture.calls == 3U, "rappels de progression complets");
    expect_true(capture.last_done == UINT16_MAX && capture.last_total == UINT16_MAX, "progression finale complète");
    expect_true(job.native_status_observed && job.native_status_source == DFX_NATIVE_STATUS_SANITIZE_LOG && (job.native_status_raw & DFX_NVME_SANITIZE_STATUS_MASK) == DFX_NVME_SANITIZE_SUCCESS, "statut natif terminal conservé avec sa source");
}

static void test_block_success_without_deallocation(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_BLOCK, 1U);
    mock.log_count = 2U;
    mock.log_progress[0] = 1200U;
    mock.log_progress[1] = UINT16_MAX;
    mock.log_status[0] = DFX_NVME_SANITIZE_IN_PROGRESS;
    mock.log_status[1] = DFX_NVME_SANITIZE_SUCCESS_NO_DEALLOCATE;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    job.native_action = DFX_NATIVE_NVME_BLOCK;
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == 0, "succès sans désallocation accepté");
    expect_true(mock.sanitize_action == DFX_NVME_SANITIZE_BLOCK, "block-erase émis au contrôleur");
    expect_true(mock.sleep_calls == 1U, "attente entre démarrage et succès");
    expect_true(mock.close_calls == 1U, "fermeture après succès sans désallocation");
}

static void test_failure_status(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.log_count = 2U;
    mock.log_status[0] = DFX_NVME_SANITIZE_IN_PROGRESS;
    mock.log_status[1] = DFX_NVME_SANITIZE_FAILED;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) != 0, "échec Sanitize remonté");
    expect_true(strstr(error, "échec de la purge") != NULL, "diagnostic d’échec Sanitize");
    expect_true(mock.sleep_calls == 1U, "attente après observation du démarrage");
    expect_true(mock.close_calls == 1U, "fermeture après échec");
}

static void test_stale_success_refused(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.log_count = DFX_NVME_PENDING_LIMIT;
    for (size_t index = 0; index < mock.log_count; index++) {
        mock.log_progress[index] = UINT16_MAX;
        mock.log_status[index] = DFX_NVME_SANITIZE_SUCCESS;
    }
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "ancien succès non attribué à la nouvelle commande");
    expect_true(strstr(error, "ancien état Sanitize") != NULL, "diagnostic de succès antérieur ambigu");
    expect_true(mock.log_index == DFX_NVME_PENDING_LIMIT, "ancien succès observé jusqu’à la limite");
    expect_true(mock.sleep_calls == DFX_NVME_PENDING_LIMIT - 1U, "attentes bornées sur ancien succès");
}

static void test_action_drift_refused(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_BLOCK, 1U);
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) != 0, "dérive de sous-méthode refusée");
    expect_true(strstr(error, "a changé depuis la planification") != NULL, "diagnostic de dérive de sous-méthode");
    expect_true(mock.sanitize_calls == 0U, "aucune commande après dérive de sous-méthode");
}

static void test_descriptor_identity_drift_refused(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.validation_result = -1;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) != 0, "dérive d’identité du descripteur refusée");
    expect_true(strstr(error, "Identité NVMe injectée différente") != NULL, "diagnostic de dérive d’identité du descripteur");
    expect_true(mock.validation_calls == 1U, "validation du descripteur appelée");
    expect_true(mock.sanitize_calls == 0U, "aucune commande après dérive d’identité du descripteur");
}

static void test_unexpected_status(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.log_count = 1U;
    mock.log_status[0] = 5U;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "état Sanitize inattendu déclaré indéterminé");
    expect_true(strstr(error, "inattendu : 5") != NULL, "diagnostic d’état inattendu");
    expect_true(mock.sleep_calls == 0U, "aucune attente après état inattendu");
    expect_true(mock.close_calls == 1U, "fermeture après état inattendu");
}

static void test_pending_limit(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.log_count = DFX_NVME_PENDING_LIMIT;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "absence de démarrage déclarée indéterminée");
    expect_true(strstr(error, "ancien état Sanitize") != NULL, "diagnostic d’absence de démarrage");
    expect_true(mock.log_index == DFX_NVME_PENDING_LIMIT, "limite de journaux respectée");
    expect_true(mock.sleep_calls == DFX_NVME_PENDING_LIMIT - 1U, "limite de temporisations respectée");
    expect_true(mock.close_calls == 1U, "fermeture après absence de démarrage");
}

static void test_tracking_deadline(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.log_count = 1U;
    mock.log_status[0] = DFX_NVME_SANITIZE_IN_PROGRESS;
    mock.repeat_last_log = true;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "suivi Sanitize borné dans le temps");
    expect_true(strstr(error, "sept jours") != NULL, "diagnostic du délai maximal de suivi");
    expect_true(mock.log_index == DFX_NVME_MAX_POLL_ROUNDS, "nombre maximal de lectures respecté");
    expect_true(mock.sleep_calls == DFX_NVME_MAX_POLL_ROUNDS - 1U, "aucune attente après la dernière lecture autorisée");
    expect_true(mock.close_calls == 1U, "fermeture après dépassement du suivi");
}

static void test_tracking_and_sleep_errors(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.admin_failure_opcode = DFX_NVME_ADMIN_GET_LOG_PAGE;
    mock.admin_failure_result = -1;
    mock.admin_system_error = EIO;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "erreur de journal déclarée indéterminée après démarrage");
    expect_true(strstr(error, "La purge a démarré") != NULL, "diagnostic de suivi après démarrage");
    expect_true(mock.close_calls == 1U, "fermeture après erreur de journal");

    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.log_count = 1U;
    mock.log_status[0] = DFX_NVME_SANITIZE_IN_PROGRESS;
    mock.sleep_result = -1;
    mock.sleep_system_error = EINTR;
    ops = make_ops(&mock);
    memset(error, 0, sizeof(error));

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "erreur de temporisation déclarée indéterminée");
    expect_true(strstr(error, "attente de suivi") != NULL, "diagnostic d’erreur de temporisation");
    expect_true(mock.close_calls == 1U, "fermeture après erreur de temporisation");
}

static void test_sanitize_submission_outcomes(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.admin_failure_opcode = DFX_NVME_ADMIN_SANITIZE;
    mock.admin_failure_result = -1;
    mock.admin_system_error = EIO;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "erreur système pendant l’envoi déclarée indéterminée");
    expect_true(strstr(error, "sans permettre d’exclure") != NULL, "diagnostic d’acceptation incertaine");
    expect_true(mock.log_index == 0U, "aucun suivi après envoi incertain");
    expect_true(mock.close_calls == 1U, "fermeture après envoi incertain");
    expect_true(!job.native_status_observed && job.native_status_source == DFX_NATIVE_STATUS_NONE, "aucun statut natif inventé après envoi incertain");

    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.admin_failure_opcode = DFX_NVME_ADMIN_SANITIZE;
    mock.admin_failure_result = 7;
    ops = make_ops(&mock);
    memset(error, 0, sizeof(error));

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) != 0, "refus explicite du contrôleur remonté");
    expect_true(strstr(error, "statut 0x7") != NULL, "diagnostic du refus explicite du contrôleur");
    expect_true(mock.log_index == 0U, "aucun suivi après refus explicite");
    expect_true(job.native_status_observed && job.native_status_source == DFX_NATIVE_STATUS_COMMAND && job.native_status_raw == 7U, "refus natif brut conservé avec sa source");
}

static void test_cancelled_tracking(void) {
    mock_nvme mock;
    memset(&mock, 0, sizeof(mock));
    configure_identify(&mock, DFX_NVME_SANICAP_CRYPTO, 1U);
    mock.cancel_on_sanitize = true;
    dfx_nvme_ops ops = make_ops(&mock);
    dfx_job job = make_job();
    char error[DFX_ERROR_MAX] = {0};

    expect_true(dfx_nvme_run_native_purge_with_ops(&job, NULL, NULL, &ops, error) == DFX_RESULT_INDETERMINATE, "suivi annulé déclaré indéterminé après acceptation");
    expect_true(mock.sanitize_calls == 1U, "commande acceptée avant annulation du suivi");
    expect_true(mock.log_index == 0U, "journal non relu après annulation");
    expect_true(strstr(error, "continue dans le contrôleur NVMe") != NULL, "diagnostic de poursuite autonome");
    expect_true(mock.close_calls == 1U, "fermeture après annulation du suivi");
}

int main(void) {
    test_action_selection();
    test_support_with_injected_ops();
    test_progress_then_success();
    test_block_success_without_deallocation();
    test_failure_status();
    test_stale_success_refused();
    test_action_drift_refused();
    test_descriptor_identity_drift_refused();
    test_unexpected_status();
    test_pending_limit();
    test_tracking_deadline();
    test_tracking_and_sleep_errors();
    test_sanitize_submission_outcomes();
    test_cancelled_tracking();
    if (failures != 0U) {
        fprintf(stderr, "%u test(s) en échec.\n", failures);
        return 1;
    }
    puts("Tests NVMe virtuels réussis.");
    return 0;
}
