#if !defined(__APPLE__) && !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "dfx.h"
#include "dfx_sha256.h"
#include "audit_internal.h"
#include "purge_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <bcrypt.h>
#define dfx_fileno _fileno
#define dfx_fsync _commit
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#define dfx_fileno fileno
#define dfx_fsync fsync
#endif

#define DFX_AUDIT_LINE_MAX 16384U
#define DFX_AUDIT_MAX_RECORDS 8192U
#define DFX_AUDIT_MAX_OPERATIONS 4096U

static void json_escape(const char *input, char *output, size_t size) {
    size_t written = 0;
    for (size_t index = 0; input[index] != '\0' && written + 1 < size; index++) {
        unsigned char value = (unsigned char)input[index];
        if ((value == '\\' || value == '"') && written + 2 < size) {
            output[written++] = '\\';
            output[written++] = (char)value;
        } else if (value >= 0x20 && value != 0x7fU) {
            output[written++] = (char)value;
        }
    }
    output[written] = '\0';
}

static FILE *open_audit(dfx_job *job, char error[DFX_ERROR_MAX]) {
#ifdef _WIN32
    HANDLE audit_handle = CreateFileA(job->audit_path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (audit_handle == INVALID_HANDLE_VALUE) {
        snprintf(error, DFX_ERROR_MAX, "Impossible d’ouvrir le journal d’audit, code Windows %lu.", GetLastError());
        return NULL;
    }
    BY_HANDLE_FILE_INFORMATION audit_information;
    if (GetFileType(audit_handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(audit_handle, &audit_information) || audit_information.nNumberOfLinks != 1U) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit doit être un fichier régulier sans lien physique supplémentaire.");
        return NULL;
    }
    FILE_ATTRIBUTE_TAG_INFO tag_information;
    if (!GetFileInformationByHandleEx(audit_handle, FileAttributeTagInfo, &tag_information, (DWORD)sizeof(tag_information)) || (tag_information.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit ne doit pas être un point de réanalyse.");
        return NULL;
    }
    uint64_t audit_index = ((uint64_t)audit_information.nFileIndexHigh << 32) | audit_information.nFileIndexLow;
    if (job->audit_identity_observed && (job->audit_identity_a != audit_information.dwVolumeSerialNumber || job->audit_identity_b != audit_index)) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Refus : le fichier de journal d’audit a changé pendant l’opération.");
        return NULL;
    }
    if (_stricmp(job->audit_path, job->device.path) == 0) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit et la cible doivent être distincts.");
        return NULL;
    }
    if (job->device.regular_file && job->device.object_identity_a == audit_information.dwVolumeSerialNumber && job->device.object_identity_b == audit_index) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit et la cible désignent le même fichier.");
        return NULL;
    }
    HANDLE target = CreateFileA(job->device.path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (target != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION target_information;
        if (GetFileInformationByHandle(target, &target_information) && audit_information.dwVolumeSerialNumber == target_information.dwVolumeSerialNumber && audit_information.nFileIndexHigh == target_information.nFileIndexHigh && audit_information.nFileIndexLow == target_information.nFileIndexLow) {
            CloseHandle(target);
            CloseHandle(audit_handle);
            snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit et la cible désignent le même fichier.");
            return NULL;
        }
        CloseHandle(target);
    }
    job->audit_identity_a = audit_information.dwVolumeSerialNumber;
    job->audit_identity_b = audit_index;
    job->audit_identity_observed = true;
    int descriptor = _open_osfhandle((intptr_t)audit_handle, _O_RDWR | _O_APPEND | _O_BINARY);
    if (descriptor < 0) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Impossible d’associer le journal d’audit : %s", strerror(errno));
        return NULL;
    }
    FILE *file = _fdopen(descriptor, "a+");
#else
    int descriptor = open(job->audit_path, O_RDWR | O_CREAT | O_APPEND | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        snprintf(error, DFX_ERROR_MAX, "Impossible d’ouvrir le journal d’audit : %s", strerror(errno));
        return NULL;
    }
    struct stat audit_status;
    if (fstat(descriptor, &audit_status) != 0 || !S_ISREG(audit_status.st_mode) || audit_status.st_nlink != 1) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit doit être un fichier régulier sans lien physique supplémentaire.");
        return NULL;
    }
    if (job->audit_identity_observed && (job->audit_identity_a != (uint64_t)audit_status.st_dev || job->audit_identity_b != (uint64_t)audit_status.st_ino)) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Refus : le fichier de journal d’audit a changé pendant l’opération.");
        return NULL;
    }
    if (job->device.regular_file && job->device.object_identity_a == (uint64_t)audit_status.st_dev && job->device.object_identity_b == (uint64_t)audit_status.st_ino) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit et la cible confirmée désignent le même fichier.");
        return NULL;
    }
    struct stat target_status;
    if (lstat(job->device.path, &target_status) == 0 && audit_status.st_dev == target_status.st_dev && audit_status.st_ino == target_status.st_ino) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit et la cible désignent le même fichier.");
        return NULL;
    }
    if (fchmod(descriptor, 0600) != 0) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Impossible de protéger les permissions du journal d’audit : %s", strerror(errno));
        return NULL;
    }
    if (flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Impossible de verrouiller le journal d’audit : %s", strerror(errno));
        return NULL;
    }
    job->audit_identity_a = (uint64_t)audit_status.st_dev;
    job->audit_identity_b = (uint64_t)audit_status.st_ino;
    job->audit_identity_observed = true;
    FILE *file = fdopen(descriptor, "a+");
#endif
    if (file == NULL) {
#ifdef _WIN32
        _close(descriptor);
#else
        close(descriptor);
#endif
        snprintf(error, DFX_ERROR_MAX, "Impossible d’associer le journal d’audit : %s", strerror(errno));
    }
    return file;
}

static bool is_hex_hash(const char *value) {
    for (size_t index = 0; index < 64; index++) {
        char character = value[index];
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'))) return false;
    }
    return value[64] == '\0';
}

static bool audit_status_valid(const char *status) {
    return status != NULL && (strcmp(status, "refusé") == 0 || strcmp(status, "en_cours") == 0 || strcmp(status, "réussi") == 0 || strcmp(status, "échoué") == 0 || strcmp(status, "indéterminé") == 0);
}

static int json_string_field(const char *line, const char *name, char *output, size_t size) {
    char marker[128];
    int marker_length = snprintf(marker, sizeof(marker), "\"%s\":\"", name);
    if (marker_length < 0 || (size_t)marker_length >= sizeof(marker)) return -1;
    const char *cursor = strstr(line, marker);
    if (cursor == NULL) return -1;
    cursor += (size_t)marker_length;
    size_t written = 0;
    bool escaped = false;
    while (*cursor != '\0') {
        if (!escaped && *cursor == '"') {
            if (written >= size) return -1;
            output[written] = '\0';
            return 0;
        }
        if (written + 1 >= size) return -1;
        output[written++] = *cursor;
        if (escaped) escaped = false;
        else if (*cursor == '\\') escaped = true;
        cursor++;
    }
    return -1;
}

static int json_scalar_field(const char *line, const char *name, char *output, size_t size) {
    char marker[128];
    int marker_length = snprintf(marker, sizeof(marker), "\"%s\":", name);
    if (marker_length < 0 || (size_t)marker_length >= sizeof(marker)) return -1;
    const char *cursor = strstr(line, marker);
    if (cursor == NULL) return -1;
    cursor += (size_t)marker_length;
    size_t written = 0;
    while (*cursor != '\0' && *cursor != ',' && *cursor != '}') {
        if (written + 1 >= size) return -1;
        output[written++] = *cursor++;
    }
    if (written == 0 || written >= size) return -1;
    output[written] = '\0';
    return 0;
}

typedef struct {
    char operation[65];
    char status[32];
    char operator_id[DFX_ID_MAX * 2];
    char witness_id[DFX_ID_MAX * 2];
    char version[64];
    char device_path[DFX_PATH_MAX * 2];
    char initial_stable_id[DFX_ID_MAX * 2];
    char final_stable_id[DFX_ID_MAX * 2];
    char qualification[DFX_ID_MAX * 2];
    uint64_t initial_size;
    uint64_t final_size;
    uint64_t block_size;
    bool final_identity_observed;
    bool lab_mode;
    bool whole_device;
    bool credible_identity;
    uint64_t native_status_raw;
    bool native_status_observed;
    bool native_method;
    bool clear_method;
    bool native_action_present;
    bool controller_verification;
    bool nvme_media;
    bool hdd_media;
    bool file_media;
    dfx_native_status_source native_status_source;
} audit_record;

static int consume_literal(const char **cursor, const char *literal) {
    size_t length = strlen(literal);
    if (strncmp(*cursor, literal, length) != 0) return -1;
    *cursor += length;
    return 0;
}

static size_t utf8_sequence_length(const unsigned char *cursor) {
    if (cursor[1] == '\0') return 0;
    if (cursor[0] >= 0xc2U && cursor[0] <= 0xdfU && cursor[1] >= 0x80U && cursor[1] <= 0xbfU) return 2;
    if (cursor[2] == '\0') return 0;
    if (cursor[0] == 0xe0U && cursor[1] >= 0xa0U && cursor[1] <= 0xbfU && cursor[2] >= 0x80U && cursor[2] <= 0xbfU) return 3;
    if (((cursor[0] >= 0xe1U && cursor[0] <= 0xecU) || (cursor[0] >= 0xeeU && cursor[0] <= 0xefU)) && cursor[1] >= 0x80U && cursor[1] <= 0xbfU && cursor[2] >= 0x80U && cursor[2] <= 0xbfU) return 3;
    if (cursor[0] == 0xedU && cursor[1] >= 0x80U && cursor[1] <= 0x9fU && cursor[2] >= 0x80U && cursor[2] <= 0xbfU) return 3;
    if (cursor[3] == '\0') return 0;
    if (cursor[0] == 0xf0U && cursor[1] >= 0x90U && cursor[1] <= 0xbfU && cursor[2] >= 0x80U && cursor[2] <= 0xbfU && cursor[3] >= 0x80U && cursor[3] <= 0xbfU) return 4;
    if (cursor[0] >= 0xf1U && cursor[0] <= 0xf3U && cursor[1] >= 0x80U && cursor[1] <= 0xbfU && cursor[2] >= 0x80U && cursor[2] <= 0xbfU && cursor[3] >= 0x80U && cursor[3] <= 0xbfU) return 4;
    if (cursor[0] == 0xf4U && cursor[1] >= 0x80U && cursor[1] <= 0x8fU && cursor[2] >= 0x80U && cursor[2] <= 0xbfU && cursor[3] >= 0x80U && cursor[3] <= 0xbfU) return 4;
    return 0;
}

static int parse_json_string(const char **cursor, char *output, size_t size) {
    if (**cursor != '"') return -1;
    (*cursor)++;
    size_t written = 0;
    while (**cursor != '\0' && **cursor != '\n') {
        unsigned char value = (unsigned char)**cursor;
        if (value == '"') {
            (*cursor)++;
            if (output != NULL) {
                if (written >= size) return -1;
                output[written] = '\0';
            }
            return 0;
        }
        size_t count = 1;
        if (value == '\\') {
            unsigned char escaped = (unsigned char)(*cursor)[1];
            if (escaped != '\\' && escaped != '"') return -1;
            count = 2;
        } else if (value < 0x20U || value == 0x7fU) return -1;
        else if (value >= 0x80U) {
            count = utf8_sequence_length((const unsigned char *)*cursor);
            if (count == 0) return -1;
        }
        if (output != NULL) {
            if (written + count >= size) return -1;
            memcpy(output + written, *cursor, count);
            written += count;
        }
        *cursor += count;
    }
    return -1;
}

static int parse_string_field(const char **cursor, const char *name, char *output, size_t size) {
    char marker[128];
    int length = snprintf(marker, sizeof(marker), ",\"%s\":", name);
    if (length < 0 || (size_t)length >= sizeof(marker) || consume_literal(cursor, marker) != 0) return -1;
    return parse_json_string(cursor, output, size);
}

static int parse_unsigned_field(const char **cursor, const char *name, uint64_t *output) {
    char marker[128];
    int length = snprintf(marker, sizeof(marker), ",\"%s\":", name);
    if (length < 0 || (size_t)length >= sizeof(marker) || consume_literal(cursor, marker) != 0) return -1;
    const char *start = *cursor;
    if (*start < '0' || *start > '9') return -1;
    if (*start == '0' && start[1] >= '0' && start[1] <= '9') return -1;
    uint64_t value = 0;
    while (**cursor >= '0' && **cursor <= '9') {
        unsigned digit = (unsigned)(**cursor - '0');
        if (value > (UINT64_MAX - digit) / 10U) return -1;
        value = value * 10U + digit;
        (*cursor)++;
    }
    if (output != NULL) *output = value;
    return 0;
}

static int parse_boolean_field(const char **cursor, const char *name, bool *output) {
    char marker[128];
    int length = snprintf(marker, sizeof(marker), ",\"%s\":", name);
    if (length < 0 || (size_t)length >= sizeof(marker) || consume_literal(cursor, marker) != 0) return -1;
    if (consume_literal(cursor, "true") == 0) {
        if (output != NULL) *output = true;
        return 0;
    }
    if (consume_literal(cursor, "false") == 0) {
        if (output != NULL) *output = false;
        return 0;
    }
    return -1;
}

static bool timestamp_valid(const char *value) {
    if (strlen(value) != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' || value[16] != ':' || value[19] != 'Z') return false;
    static const size_t digits[] = {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    for (size_t index = 0; index < sizeof(digits) / sizeof(digits[0]); index++) {
        if (value[digits[index]] < '0' || value[digits[index]] > '9') return false;
    }
    unsigned year = (unsigned)(value[0] - '0') * 1000U + (unsigned)(value[1] - '0') * 100U + (unsigned)(value[2] - '0') * 10U + (unsigned)(value[3] - '0');
    unsigned month = (unsigned)(value[5] - '0') * 10U + (unsigned)(value[6] - '0');
    unsigned day = (unsigned)(value[8] - '0') * 10U + (unsigned)(value[9] - '0');
    unsigned hour = (unsigned)(value[11] - '0') * 10U + (unsigned)(value[12] - '0');
    unsigned minute = (unsigned)(value[14] - '0') * 10U + (unsigned)(value[15] - '0');
    unsigned second = (unsigned)(value[17] - '0') * 10U + (unsigned)(value[18] - '0');
    static const unsigned days_per_month[] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    if (year < 1970U || month < 1U || month > 12U || hour > 23U || minute > 59U || second > 60U) return false;
    unsigned maximum_day = days_per_month[month - 1U];
    if (month == 2U && (year % 400U == 0U || (year % 4U == 0U && year % 100U != 0U))) maximum_day = 29U;
    return day >= 1U && day <= maximum_day;
}

static bool method_value_valid(const char *value) {
    return strcmp(value, "auto") == 0 || strcmp(value, "clear-zero") == 0 || strcmp(value, "purge-native") == 0 || strcmp(value, "destruction-physique") == 0;
}

static int parse_audit_record(const char *line, audit_record *record) {
    const char *cursor = line;
    char timestamp[32] = {0};
    char requested_method[32] = {0};
    char executed_method[32] = {0};
    char native_action[32] = {0};
    char native_status_source[32] = {0};
    char verification[32] = {0};
    char media_kind[32] = {0};
    char previous[65] = {0};
    char hash[65] = {0};
    memset(record, 0, sizeof(*record));
    if (consume_literal(&cursor, "{\"schéma\":1") != 0
        || parse_string_field(&cursor, "horodatage", timestamp, sizeof(timestamp)) != 0
        || parse_string_field(&cursor, "opération", record->operation, sizeof(record->operation)) != 0
        || parse_string_field(&cursor, "opérateur", record->operator_id, sizeof(record->operator_id)) != 0
        || parse_string_field(&cursor, "témoin", record->witness_id, sizeof(record->witness_id)) != 0
        || parse_string_field(&cursor, "version", record->version, sizeof(record->version)) != 0
        || parse_string_field(&cursor, "périphérique", record->device_path, sizeof(record->device_path)) != 0
        || parse_string_field(&cursor, "identifiant", record->initial_stable_id, sizeof(record->initial_stable_id)) != 0
        || parse_string_field(&cursor, "identifiant_après", record->final_stable_id, sizeof(record->final_stable_id)) != 0
        || parse_unsigned_field(&cursor, "taille_après", &record->final_size) != 0
        || parse_boolean_field(&cursor, "identité_après_observée", &record->final_identity_observed) != 0
        || parse_string_field(&cursor, "modèle", NULL, 0) != 0
        || parse_string_field(&cursor, "firmware", NULL, 0) != 0
        || parse_string_field(&cursor, "transport", NULL, 0) != 0
        || parse_string_field(&cursor, "environnement", NULL, 0) != 0
        || parse_string_field(&cursor, "topologie", NULL, 0) != 0
        || parse_string_field(&cursor, "qualification", record->qualification, sizeof(record->qualification)) != 0
        || parse_boolean_field(&cursor, "laboratoire", &record->lab_mode) != 0
        || parse_unsigned_field(&cursor, "taille", &record->initial_size) != 0
        || parse_unsigned_field(&cursor, "taille_bloc", &record->block_size) != 0
        || parse_string_field(&cursor, "type_support", media_kind, sizeof(media_kind)) != 0
        || parse_boolean_field(&cursor, "disque_entier", &record->whole_device) != 0
        || parse_boolean_field(&cursor, "identité_unique", &record->credible_identity) != 0
        || parse_string_field(&cursor, "méthode_demandée", requested_method, sizeof(requested_method)) != 0
        || parse_string_field(&cursor, "méthode_exécutée", executed_method, sizeof(executed_method)) != 0
        || parse_string_field(&cursor, "sous_méthode_native", native_action, sizeof(native_action)) != 0
        || parse_string_field(&cursor, "source_statut_natif", native_status_source, sizeof(native_status_source)) != 0
        || parse_boolean_field(&cursor, "statut_natif_observé", &record->native_status_observed) != 0
        || parse_unsigned_field(&cursor, "statut_natif_brut", &record->native_status_raw) != 0
        || parse_string_field(&cursor, "vérification", verification, sizeof(verification)) != 0
        || parse_string_field(&cursor, "statut", record->status, sizeof(record->status)) != 0
        || parse_string_field(&cursor, "détail", NULL, 0) != 0
        || parse_string_field(&cursor, "précédente", previous, sizeof(previous)) != 0
        || parse_string_field(&cursor, "empreinte", hash, sizeof(hash)) != 0
        || consume_literal(&cursor, "}\n") != 0
        || *cursor != '\0') return -1;
    if (!timestamp_valid(timestamp) || !is_hex_hash(record->operation) || !is_hex_hash(previous) || !is_hex_hash(hash) || !method_value_valid(requested_method) || (strcmp(executed_method, "clear-zero") != 0 && strcmp(executed_method, "purge-native") != 0)) return -1;
    if (strcmp(native_action, "aucune") != 0 && strcmp(native_action, "nvme-block-erase") != 0 && strcmp(native_action, "nvme-crypto-erase") != 0) return -1;
    if (strcmp(native_status_source, "aucune") == 0) record->native_status_source = DFX_NATIVE_STATUS_NONE;
    else if (strcmp(native_status_source, "nvme-commande") == 0) record->native_status_source = DFX_NATIVE_STATUS_COMMAND;
    else if (strcmp(native_status_source, "nvme-journal-sanitize") == 0) record->native_status_source = DFX_NATIVE_STATUS_SANITIZE_LOG;
    else return -1;
    if (strcmp(media_kind, "HDD") != 0 && strcmp(media_kind, "SSD") != 0 && strcmp(media_kind, "NVMe") != 0 && strcmp(media_kind, "flash") != 0 && strcmp(media_kind, "fichier") != 0 && strcmp(media_kind, "inconnu") != 0) return -1;
    if (strcmp(verification, "complète") != 0 && strcmp(verification, "échantillonnée") != 0 && strcmp(verification, "contrôleur") != 0) return -1;
    record->native_method = strcmp(executed_method, "purge-native") == 0;
    record->clear_method = strcmp(executed_method, "clear-zero") == 0;
    record->native_action_present = strcmp(native_action, "aucune") != 0;
    record->controller_verification = strcmp(verification, "contrôleur") == 0;
    record->nvme_media = strcmp(media_kind, "NVMe") == 0;
    record->hdd_media = strcmp(media_kind, "HDD") == 0;
    record->file_media = strcmp(media_kind, "fichier") == 0;
    return audit_status_valid(record->status) ? 0 : -1;
}

bool dfx_audit_record_syntax_valid(const unsigned char *data, size_t size) {
    if (data == NULL || size == 0 || size >= DFX_AUDIT_LINE_MAX || memchr(data, '\0', size) != NULL) return false;
    char *line = malloc(size + 1U);
    if (line == NULL) return false;
    memcpy(line, data, size);
    line[size] = '\0';
    audit_record record;
    bool valid = parse_audit_record(line, &record) == 0;
    free(line);
    return valid;
}

static const char *audit_record_semantic_error(const audit_record *record) {
    bool refused = strcmp(record->status, "refusé") == 0;
    bool final_id_present = record->final_stable_id[0] != '\0';
    if (record->final_identity_observed != final_id_present || (!record->final_identity_observed && record->final_size != 0) || (strcmp(record->status, "réussi") == 0 && !record->final_identity_observed) || ((strcmp(record->status, "en_cours") == 0 || strcmp(record->status, "refusé") == 0) && record->final_identity_observed)) return "Identité finale incohérente";
    if (record->version[0] == '\0' || record->device_path[0] == '\0' || record->initial_stable_id[0] == '\0') return "Contexte d’identité initiale incomplet";
    if (record->final_identity_observed && record->final_size != record->initial_size) return "Capacité finale incohérente";
    if (record->final_identity_observed && !record->file_media && strcmp(record->initial_stable_id, record->final_stable_id) != 0) return "Identité matérielle finale incohérente";
    if (record->native_method != record->native_action_present || record->native_method != record->controller_verification || (record->native_method && !record->nvme_media)) return "Contexte de méthode native incohérent";
    if (record->native_status_raw > UINT32_MAX || record->native_status_observed != (record->native_status_source != DFX_NATIVE_STATUS_NONE) || (!record->native_status_observed && record->native_status_raw != 0) || (!record->native_method && record->native_status_observed) || ((strcmp(record->status, "en_cours") == 0 || strcmp(record->status, "refusé") == 0) && record->native_status_observed)) return "Statut natif incohérent";
    unsigned native_state = (unsigned)(record->native_status_raw & DFX_NVME_SANITIZE_STATUS_MASK);
    if (strcmp(record->status, "réussi") == 0 && record->native_method && (!record->native_status_observed || record->native_status_source != DFX_NATIVE_STATUS_SANITIZE_LOG || (native_state != DFX_NVME_SANITIZE_SUCCESS && native_state != DFX_NVME_SANITIZE_SUCCESS_NO_DEALLOCATE))) return "Statut natif terminal incohérent";
    if (record->native_status_source == DFX_NATIVE_STATUS_COMMAND && strcmp(record->status, "réussi") == 0) return "Source du statut natif incohérente";
    if (!refused) {
        if (!record->whole_device || !record->credible_identity) return "Portée ou identité destructive incohérente";
        if (record->file_media) {
            if (!record->clear_method || record->block_size != 1U || strcmp(record->qualification, "VIRTUEL-TEST") != 0) return "Contexte de fichier virtuel incohérent";
        } else {
            if (record->initial_size == 0 || record->block_size == 0 || record->initial_size % record->block_size != 0) return "Géométrie physique incohérente";
            if (record->operator_id[0] == '\0' || record->witness_id[0] == '\0' || strcmp(record->operator_id, record->witness_id) == 0) return "Double contrôle physique incohérent";
            if (!record->lab_mode && record->qualification[0] == '\0') return "Qualification physique absente";
            if (record->clear_method && (!record->hdd_media || record->controller_verification)) return "Méthode clear incohérente avec le support";
        }
    }
    return NULL;
}

static bool bounded_text_safe(const char *text, size_t size) {
    return text != NULL && memchr(text, '\0', size) != NULL && dfx_text_is_safe(text);
}

static bool limited_text_safe(const char *text, size_t maximum) {
    if (text == NULL) return false;
    size_t length = 0;
    while (length < maximum && text[length] != '\0') length++;
    return length < maximum && dfx_text_is_safe(text);
}

static bool audit_inputs_safe(const dfx_job *job, const char *detail) {
    return job != NULL
        && bounded_text_safe(job->audit_path, sizeof(job->audit_path))
        && bounded_text_safe(job->device.path, sizeof(job->device.path))
        && bounded_text_safe(job->device.stable_id, sizeof(job->device.stable_id))
        && bounded_text_safe(job->final_stable_id, sizeof(job->final_stable_id))
        && bounded_text_safe(job->device.model, sizeof(job->device.model))
        && bounded_text_safe(job->device.firmware, sizeof(job->device.firmware))
        && bounded_text_safe(job->device.transport, sizeof(job->device.transport))
        && bounded_text_safe(job->device.environment, sizeof(job->device.environment))
        && bounded_text_safe(job->device.topology, sizeof(job->device.topology))
        && bounded_text_safe(job->device.qualification_id, sizeof(job->device.qualification_id))
        && bounded_text_safe(job->operator_id, sizeof(job->operator_id))
        && bounded_text_safe(job->witness_id, sizeof(job->witness_id))
        && limited_text_safe(detail, DFX_ERROR_MAX);
}

typedef struct {
    char operation[65];
    char context_hash[65];
    bool running;
    bool terminal;
} audit_operation;

static int audit_context_hash(const char *line, char output[65]) {
    static const char *string_fields[] = {
        "opérateur", "témoin", "version", "périphérique", "identifiant", "modèle", "firmware", "transport", "environnement", "topologie", "qualification", "type_support", "méthode_demandée", "méthode_exécutée", "sous_méthode_native", "vérification"
    };
    char material[8192] = {0};
    size_t used = 0;
    for (size_t index = 0; index < sizeof(string_fields) / sizeof(string_fields[0]); index++) {
        char value[DFX_PATH_MAX * 2];
        if (json_string_field(line, string_fields[index], value, sizeof(value)) != 0) return -1;
        int count = snprintf(material + used, sizeof(material) - used, "%s=%s\n", string_fields[index], value);
        if (count < 0 || (size_t)count >= sizeof(material) - used) return -1;
        used += (size_t)count;
    }
    static const char *scalar_fields[] = {"schéma", "laboratoire", "taille", "taille_bloc", "disque_entier", "identité_unique"};
    for (size_t index = 0; index < sizeof(scalar_fields) / sizeof(scalar_fields[0]); index++) {
        char value[128];
        if (json_scalar_field(line, scalar_fields[index], value, sizeof(value)) != 0) return -1;
        int count = snprintf(material + used, sizeof(material) - used, "%s=%s\n", scalar_fields[index], value);
        if (count < 0 || (size_t)count >= sizeof(material) - used) return -1;
        used += (size_t)count;
    }
    dfx_sha256_hex((const unsigned char *)material, used, output);
    return 0;
}

static int validate_semantics(FILE *file, const char *allowed_running_operation, const char *allowed_context_hash, const char *forbidden_existing_operation, size_t *operation_count_output, char error[DFX_ERROR_MAX]) {
    if (fseek(file, 0, SEEK_SET) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Lecture sémantique du journal impossible : %s", strerror(errno));
        return -1;
    }
    audit_operation *operations = NULL;
    size_t operation_count = 0;
    char active_operation[65] = {0};
    char line[DFX_AUDIT_LINE_MAX];
    unsigned long line_number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;
        if (line_number > DFX_AUDIT_MAX_RECORDS) {
            free(operations);
            snprintf(error, DFX_ERROR_MAX, "Le journal dépasse la limite de %u enregistrements.", DFX_AUDIT_MAX_RECORDS);
            return -1;
        }
        audit_record record;
        char context_hash[65];
        if (parse_audit_record(line, &record) != 0 || audit_context_hash(line, context_hash) != 0) {
            free(operations);
            snprintf(error, DFX_ERROR_MAX, "Schéma d’audit invalide à la ligne %lu.", line_number);
            return -1;
        }
        const char *semantic_error = audit_record_semantic_error(&record);
        if (semantic_error != NULL) {
            free(operations);
            snprintf(error, DFX_ERROR_MAX, "%s à la ligne %lu.", semantic_error, line_number);
            return -1;
        }
        size_t operation_index = 0;
        while (operation_index < operation_count && strcmp(operations[operation_index].operation, record.operation) != 0) operation_index++;
        if (operation_index == operation_count) {
            if (operation_count >= DFX_AUDIT_MAX_OPERATIONS) {
                free(operations);
                snprintf(error, DFX_ERROR_MAX, "Le journal dépasse la limite de %u opérations.", DFX_AUDIT_MAX_OPERATIONS);
                return -1;
            }
            audit_operation *resized = realloc(operations, (operation_count + 1) * sizeof(*operations));
            if (resized == NULL) {
                free(operations);
                snprintf(error, DFX_ERROR_MAX, "Mémoire insuffisante pendant la validation du journal.");
                return -1;
            }
            operations = resized;
            memset(&operations[operation_count], 0, sizeof(operations[operation_count]));
            snprintf(operations[operation_count].operation, sizeof(operations[operation_count].operation), "%s", record.operation);
            snprintf(operations[operation_count].context_hash, sizeof(operations[operation_count].context_hash), "%s", context_hash);
            operation_index = operation_count++;
        } else if (strcmp(operations[operation_index].context_hash, context_hash) != 0) {
            free(operations);
            snprintf(error, DFX_ERROR_MAX, "Contexte modifié pour l’opération de la ligne %lu.", line_number);
            return -1;
        }
        audit_operation *current = &operations[operation_index];
        if (strcmp(record.status, "refusé") == 0) {
            if (current->running || current->terminal) {
                free(operations);
                snprintf(error, DFX_ERROR_MAX, "Transition refusée invalide à la ligne %lu.", line_number);
                return -1;
            }
            current->terminal = true;
        } else if (strcmp(record.status, "en_cours") == 0) {
            if (current->running || current->terminal || active_operation[0] != '\0') {
                free(operations);
                snprintf(error, DFX_ERROR_MAX, "Démarrage dupliqué ou opération concurrente à la ligne %lu.", line_number);
                return -1;
            }
            current->running = true;
            snprintf(active_operation, sizeof(active_operation), "%s", record.operation);
        } else {
            if (!current->running || current->terminal || strcmp(active_operation, record.operation) != 0) {
                free(operations);
                snprintf(error, DFX_ERROR_MAX, "État terminal sans démarrage unique à la ligne %lu.", line_number);
                return -1;
            }
            current->running = false;
            current->terminal = true;
            active_operation[0] = '\0';
        }
    }
    if (ferror(file)) {
        free(operations);
        snprintf(error, DFX_ERROR_MAX, "Lecture sémantique du journal interrompue.");
        return -1;
    }
    bool allowed_running_found = false;
    for (size_t index = 0; index < operation_count; index++) {
        if (forbidden_existing_operation != NULL && strcmp(operations[index].operation, forbidden_existing_operation) == 0) {
            snprintf(error, DFX_ERROR_MAX, "L’identifiant d’opération existe déjà dans le journal.");
            free(operations);
            return -1;
        }
        if (operations[index].running && allowed_running_operation != NULL && strcmp(operations[index].operation, allowed_running_operation) == 0) {
            if (allowed_context_hash == NULL || strcmp(operations[index].context_hash, allowed_context_hash) != 0) {
                snprintf(error, DFX_ERROR_MAX, "Contexte modifié pour l’opération terminale.");
                free(operations);
                return -1;
            }
            allowed_running_found = true;
            continue;
        }
        if (operations[index].running || !operations[index].terminal) {
            snprintf(error, DFX_ERROR_MAX, "L’opération %s ne possède pas d’état terminal.", operations[index].operation);
            free(operations);
            return -1;
        }
    }
    if (allowed_running_operation != NULL && !allowed_running_found) {
        snprintf(error, DFX_ERROR_MAX, "Aucun démarrage compatible n’existe pour l’opération terminale.");
        free(operations);
        return -1;
    }
    if (operation_count_output != NULL) *operation_count_output = operation_count;
    free(operations);
    return 0;
}

static int validate_chain(FILE *file, char previous[65], unsigned long *record_count, char error[DFX_ERROR_MAX]) {
    memset(previous, '0', 64);
    previous[64] = '\0';
    if (fseek(file, 0, SEEK_SET) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Lecture du journal d’audit impossible : %s", strerror(errno));
        return -1;
    }
    char line[DFX_AUDIT_LINE_MAX];
    unsigned long line_number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;
        if (line_number > DFX_AUDIT_MAX_RECORDS) {
            snprintf(error, DFX_ERROR_MAX, "Le journal dépasse la limite de %u enregistrements.", DFX_AUDIT_MAX_RECORDS);
            return -1;
        }
        size_t length = strlen(line);
        if (length == 0 || line[length - 1] != '\n') {
            snprintf(error, DFX_ERROR_MAX, "Journal d’audit tronqué à la ligne %lu.", line_number);
            return -1;
        }
        line[--length] = '\0';
        const char *marker = strstr(line, ",\"précédente\":\"");
        if (marker == NULL) {
            snprintf(error, DFX_ERROR_MAX, "Chaîne d’audit absente à la ligne %lu.", line_number);
            return -1;
        }
        const char *stored_previous = marker + strlen(",\"précédente\":\"");
        const char *hash_separator = "\",\"empreinte\":\"";
        size_t expected_suffix_length = 64 + strlen(hash_separator) + 64 + 2;
        if (strlen(stored_previous) != expected_suffix_length || strncmp(stored_previous, previous, 64) != 0 || strncmp(stored_previous + 64, hash_separator, strlen(hash_separator)) != 0) {
            snprintf(error, DFX_ERROR_MAX, "Chaîne d’audit rompue à la ligne %lu.", line_number);
            return -1;
        }
        const char *hash_marker = stored_previous + 64 + strlen(hash_separator);
        char stored_hash[65];
        memcpy(stored_hash, hash_marker, 64);
        stored_hash[64] = '\0';
        if (!is_hex_hash(stored_hash) || hash_marker[64] != '"' || hash_marker[65] != '}' || hash_marker[66] != '\0') {
            snprintf(error, DFX_ERROR_MAX, "Empreinte d’audit invalide à la ligne %lu.", line_number);
            return -1;
        }
        size_t payload_length = (size_t)(marker - line);
        char payload[DFX_AUDIT_LINE_MAX];
        if (payload_length + 2 > sizeof(payload)) {
            snprintf(error, DFX_ERROR_MAX, "Enregistrement d’audit trop long à la ligne %lu.", line_number);
            return -1;
        }
        memcpy(payload, line, payload_length);
        payload[payload_length++] = '}';
        payload[payload_length] = '\0';
        char material[DFX_AUDIT_LINE_MAX * 2U];
        int material_length = snprintf(material, sizeof(material), "%s%s", previous, payload);
        if (material_length < 0 || (size_t)material_length >= sizeof(material)) {
            snprintf(error, DFX_ERROR_MAX, "Enregistrement d’audit trop long à la ligne %lu.", line_number);
            return -1;
        }
        char expected[65];
        dfx_sha256_hex((const unsigned char *)material, (size_t)material_length, expected);
        if (strcmp(expected, stored_hash) != 0) {
            snprintf(error, DFX_ERROR_MAX, "Journal d’audit modifié à la ligne %lu.", line_number);
            return -1;
        }
        snprintf(previous, 65, "%s", stored_hash);
    }
    if (ferror(file)) {
        snprintf(error, DFX_ERROR_MAX, "Lecture du journal d’audit interrompue.");
        return -1;
    }
    if (record_count != NULL) *record_count = line_number;
    return 0;
}

int dfx_prepare_audit(dfx_job *job, char error[DFX_ERROR_MAX]) {
    if (job == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Tâche d’audit absente.");
        return -1;
    }
    if (job->operation_id[0] != '\0') return 0;
    unsigned char random_bytes[32];
#ifdef _WIN32
    if (BCryptGenRandom(NULL, random_bytes, (ULONG)sizeof(random_bytes), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Impossible de générer un identifiant d’opération cryptographiquement aléatoire.");
        return -1;
    }
#else
    int descriptor = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        snprintf(error, DFX_ERROR_MAX, "Impossible d’accéder à la source aléatoire du système : %s", strerror(errno));
        return -1;
    }
    size_t received = 0;
    while (received < sizeof(random_bytes)) {
        ssize_t count = read(descriptor, random_bytes + received, sizeof(random_bytes) - received);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
            int saved_error = count < 0 ? errno : EIO;
            close(descriptor);
            snprintf(error, DFX_ERROR_MAX, "Lecture de la source aléatoire impossible : %s", strerror(saved_error));
            return -1;
        }
        received += (size_t)count;
    }
    if (close(descriptor) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Fermeture de la source aléatoire impossible : %s", strerror(errno));
        return -1;
    }
#endif
    dfx_sha256_hex(random_bytes, sizeof(random_bytes), job->operation_id);
    memset(random_bytes, 0, sizeof(random_bytes));
    return 0;
}

int dfx_write_audit(dfx_job *job, const char *status, const char *detail, char error[DFX_ERROR_MAX]) {
    if (job == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Tâche d’audit absente.");
        return -1;
    }
    if (job->audit_path[0] == '\0') return 0;
    if (!audit_status_valid(status)) {
        snprintf(error, DFX_ERROR_MAX, "État d’audit invalide.");
        return -1;
    }
    if (!audit_inputs_safe(job, detail)) {
        snprintf(error, DFX_ERROR_MAX, "Texte d’audit non terminé, contenant un contrôle ou un UTF-8 invalide.");
        return -1;
    }
    if (!is_hex_hash(job->operation_id)) {
        snprintf(error, DFX_ERROR_MAX, "Identifiant d’opération absent ou invalide.");
        return -1;
    }
    FILE *file = open_audit(job, error);
    if (file == NULL) return -1;
    char previous[65];
    unsigned long existing_records = 0;
    if (validate_chain(file, previous, &existing_records, error) != 0) {
        fclose(file);
        return -1;
    }
    char path[DFX_PATH_MAX * 2];
    char stable_id[DFX_ID_MAX * 2];
    char final_stable_id[DFX_ID_MAX * 2];
    char model[DFX_MODEL_MAX * 2];
    char firmware[DFX_DETAIL_MAX * 2];
    char transport[DFX_DETAIL_MAX * 2];
    char environment[DFX_DETAIL_MAX * 2];
    char topology[DFX_ID_MAX * 2];
    char qualification[DFX_ID_MAX * 2];
    char operator_id[DFX_ID_MAX * 2];
    char witness_id[DFX_ID_MAX * 2];
    char escaped_detail[DFX_ERROR_MAX * 2];
    json_escape(job->device.path, path, sizeof(path));
    json_escape(job->device.stable_id, stable_id, sizeof(stable_id));
    json_escape(job->final_stable_id, final_stable_id, sizeof(final_stable_id));
    json_escape(job->device.model, model, sizeof(model));
    json_escape(job->device.firmware, firmware, sizeof(firmware));
    json_escape(job->device.transport, transport, sizeof(transport));
    json_escape(job->device.environment, environment, sizeof(environment));
    json_escape(job->device.topology, topology, sizeof(topology));
    json_escape(job->device.qualification_id, qualification, sizeof(qualification));
    json_escape(job->operator_id, operator_id, sizeof(operator_id));
    json_escape(job->witness_id, witness_id, sizeof(witness_id));
    json_escape(detail, escaped_detail, sizeof(escaped_detail));
    time_t now = time(NULL);
    struct tm utc;
#ifdef _WIN32
    if (now == (time_t)-1 || gmtime_s(&utc, &now) != 0) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Horloge UTC indisponible pour le journal d’audit.");
        return -1;
    }
#else
    if (now == (time_t)-1 || gmtime_r(&now, &utc) == NULL) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Horloge UTC indisponible pour le journal d’audit.");
        return -1;
    }
#endif
    char timestamp[32];
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Formatage UTC impossible pour le journal d’audit.");
        return -1;
    }
    char payload[DFX_AUDIT_LINE_MAX];
    int payload_length = snprintf(payload, sizeof(payload),
        "{\"schéma\":1,\"horodatage\":\"%s\",\"opération\":\"%s\",\"opérateur\":\"%s\",\"témoin\":\"%s\",\"version\":\"%s\",\"périphérique\":\"%s\",\"identifiant\":\"%s\",\"identifiant_après\":\"%s\",\"taille_après\":%llu,\"identité_après_observée\":%s,\"modèle\":\"%s\",\"firmware\":\"%s\",\"transport\":\"%s\",\"environnement\":\"%s\",\"topologie\":\"%s\",\"qualification\":\"%s\",\"laboratoire\":%s,\"taille\":%llu,\"taille_bloc\":%u,\"type_support\":\"%s\",\"disque_entier\":%s,\"identité_unique\":%s,\"méthode_demandée\":\"%s\",\"méthode_exécutée\":\"%s\",\"sous_méthode_native\":\"%s\",\"source_statut_natif\":\"%s\",\"statut_natif_observé\":%s,\"statut_natif_brut\":%u,\"vérification\":\"%s\",\"statut\":\"%s\",\"détail\":\"%s\"}",
        timestamp, job->operation_id, operator_id, witness_id, DFX_BUILD_VERSION, path, stable_id, final_stable_id, (unsigned long long)job->final_size_bytes, job->final_identity_observed ? "true" : "false", model, firmware, transport, environment, topology, qualification, job->lab_mode ? "true" : "false",
        (unsigned long long)job->device.size_bytes, job->device.logical_block_size, dfx_media_name(job->device.kind), job->device.whole_device ? "true" : "false", job->device.stable_identity_unique ? "true" : "false", dfx_method_name(job->requested_method), dfx_method_name(job->method), dfx_native_action_name(job->native_action), dfx_native_status_source_name(job->native_status_source), job->native_status_observed ? "true" : "false", job->native_status_raw,
        job->method == DFX_METHOD_PURGE_NATIVE ? "contrôleur" : (job->full_verify ? "complète" : "échantillonnée"), status, escaped_detail);
    if (payload_length < 0 || (size_t)payload_length >= sizeof(payload)) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Enregistrement d’audit trop long.");
        return -1;
    }
    char material[DFX_AUDIT_LINE_MAX * 2U];
    int material_length = snprintf(material, sizeof(material), "%s%s", previous, payload);
    if (material_length < 0 || (size_t)material_length >= sizeof(material)) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Enregistrement d’audit trop long.");
        return -1;
    }
    char hash[65];
    dfx_sha256_hex((const unsigned char *)material, (size_t)material_length, hash);
    char record_line[DFX_AUDIT_LINE_MAX];
    int record_length = snprintf(record_line, sizeof(record_line), "%.*s,\"précédente\":\"%s\",\"empreinte\":\"%s\"}\n", payload_length - 1, payload, previous, hash);
    audit_record parsed_record;
    char context_hash[65];
    if (record_length < 0 || (size_t)record_length >= sizeof(record_line) || parse_audit_record(record_line, &parsed_record) != 0 || audit_context_hash(record_line, context_hash) != 0) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Enregistrement d’audit non canonique ou invalide.");
        return -1;
    }
    const char *semantic_error = audit_record_semantic_error(&parsed_record);
    if (semantic_error != NULL) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "%s dans l’enregistrement à écrire.", semantic_error);
        return -1;
    }
    bool terminal = strcmp(status, "réussi") == 0 || strcmp(status, "échoué") == 0 || strcmp(status, "indéterminé") == 0;
    size_t existing_operations = 0;
    if (validate_semantics(file, terminal ? job->operation_id : NULL, terminal ? context_hash : NULL, terminal ? NULL : job->operation_id, &existing_operations, error) != 0) {
        fclose(file);
        return -1;
    }
    bool running = strcmp(status, "en_cours") == 0;
    unsigned long required_records = running ? 2UL : 1UL;
    if (existing_records > DFX_AUDIT_MAX_RECORDS - required_records || (!terminal && existing_operations >= DFX_AUDIT_MAX_OPERATIONS)) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Capacité restante du journal insuffisante pour cette opération et son état terminal.");
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0 || fwrite(record_line, 1, (size_t)record_length, file) != (size_t)record_length || fflush(file) != 0 || dfx_fsync(dfx_fileno(file)) != 0) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Échec d’écriture synchronisée du journal d’audit : %s", strerror(errno));
        return -1;
    }
    if (fclose(file) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Fermeture du journal d’audit impossible : %s", strerror(errno));
        return -1;
    }
    return 0;
}

int dfx_verify_audit_file(const char *path, FILE *out, char error[DFX_ERROR_MAX]) {
    if (path == NULL || path[0] == '\0' || strlen(path) >= DFX_PATH_MAX || !dfx_text_is_safe(path) || out == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Chemin ou sortie de vérification d’audit invalide.");
        return -1;
    }
#ifdef _WIN32
    HANDLE audit_handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (audit_handle == INVALID_HANDLE_VALUE) {
        snprintf(error, DFX_ERROR_MAX, "Impossible d’ouvrir le journal d’audit, code Windows %lu.", GetLastError());
        return -1;
    }
    FILE_ATTRIBUTE_TAG_INFO tag_information;
    BY_HANDLE_FILE_INFORMATION audit_information;
    if (GetFileType(audit_handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(audit_handle, &audit_information) || audit_information.nNumberOfLinks != 1U || !GetFileInformationByHandleEx(audit_handle, FileAttributeTagInfo, &tag_information, (DWORD)sizeof(tag_information)) || (tag_information.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit doit être un fichier régulier sans alias de chemin.");
        return -1;
    }
    int descriptor = _open_osfhandle((intptr_t)audit_handle, _O_RDONLY | _O_BINARY);
    if (descriptor < 0) {
        CloseHandle(audit_handle);
        snprintf(error, DFX_ERROR_MAX, "Impossible d’associer le journal d’audit : %s", strerror(errno));
        return -1;
    }
    FILE *file = _fdopen(descriptor, "r");
#else
    int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor < 0) {
        snprintf(error, DFX_ERROR_MAX, "Impossible d’ouvrir le journal d’audit : %s", strerror(errno));
        return -1;
    }
    struct stat status;
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) || status.st_nlink != 1) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Refus : le journal d’audit doit être un fichier régulier sans alias de chemin.");
        return -1;
    }
    if (flock(descriptor, LOCK_SH | LOCK_NB) != 0) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Impossible de verrouiller le journal d’audit pour sa vérification : %s", strerror(errno));
        return -1;
    }
    FILE *file = fdopen(descriptor, "r");
#endif
    if (file == NULL) {
#ifdef _WIN32
        _close(descriptor);
#else
        close(descriptor);
#endif
        snprintf(error, DFX_ERROR_MAX, "Lecture du journal d’audit impossible : %s", strerror(errno));
        return -1;
    }
    char last_hash[65];
    unsigned long records = 0;
    int validation = validate_chain(file, last_hash, &records, error);
    if (validation != 0) {
        fclose(file);
        return -1;
    }
    if (records == 0) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Le journal d’audit est vide.");
        return -1;
    }
    if (validate_semantics(file, NULL, NULL, NULL, NULL, error) != 0) {
        fclose(file);
        return -1;
    }
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION final_information;
    if (!GetFileInformationByHandle(audit_handle, &final_information) || audit_information.dwVolumeSerialNumber != final_information.dwVolumeSerialNumber || audit_information.nFileIndexHigh != final_information.nFileIndexHigh || audit_information.nFileIndexLow != final_information.nFileIndexLow || audit_information.nFileSizeHigh != final_information.nFileSizeHigh || audit_information.nFileSizeLow != final_information.nFileSizeLow || audit_information.ftLastWriteTime.dwHighDateTime != final_information.ftLastWriteTime.dwHighDateTime || audit_information.ftLastWriteTime.dwLowDateTime != final_information.ftLastWriteTime.dwLowDateTime || final_information.nNumberOfLinks != 1U) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Le journal d’audit a changé pendant sa vérification.");
        return -1;
    }
#else
    struct stat final_status;
    bool unchanged = fstat(descriptor, &final_status) == 0 && status.st_dev == final_status.st_dev && status.st_ino == final_status.st_ino && status.st_size == final_status.st_size && final_status.st_nlink == 1;
#ifdef __APPLE__
    unchanged = unchanged && status.st_mtimespec.tv_sec == final_status.st_mtimespec.tv_sec && status.st_mtimespec.tv_nsec == final_status.st_mtimespec.tv_nsec && status.st_ctimespec.tv_sec == final_status.st_ctimespec.tv_sec && status.st_ctimespec.tv_nsec == final_status.st_ctimespec.tv_nsec;
#else
    unchanged = unchanged && status.st_mtim.tv_sec == final_status.st_mtim.tv_sec && status.st_mtim.tv_nsec == final_status.st_mtim.tv_nsec && status.st_ctim.tv_sec == final_status.st_ctim.tv_sec && status.st_ctim.tv_nsec == final_status.st_ctim.tv_nsec;
#endif
    if (!unchanged) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Le journal d’audit a changé pendant sa vérification.");
        return -1;
    }
#endif
    if (fclose(file) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Fermeture du journal d’audit impossible : %s", strerror(errno));
        return -1;
    }
    fprintf(out, "Journal valide : %lu enregistrement%s\n", records, records > 1 ? "s" : "");
    fprintf(out, "Empreinte finale : %s\n", last_hash);
    return 0;
}
