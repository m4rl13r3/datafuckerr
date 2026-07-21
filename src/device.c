#define _POSIX_C_SOURCE 200809L

#include "dfx.h"

#include <string.h>

const char *dfx_media_name(dfx_media_kind kind) {
    switch (kind) {
        case DFX_MEDIA_HDD: return "HDD";
        case DFX_MEDIA_SSD: return "SSD";
        case DFX_MEDIA_NVME: return "NVMe";
        case DFX_MEDIA_FLASH: return "flash";
        case DFX_MEDIA_FILE: return "fichier";
        default: return "inconnu";
    }
}

const char *dfx_method_name(dfx_method method) {
    switch (method) {
        case DFX_METHOD_AUTO: return "auto";
        case DFX_METHOD_CLEAR_ZERO: return "clear-zero";
        case DFX_METHOD_PURGE_NATIVE: return "purge-native";
        case DFX_METHOD_DESTROY_PHYSICAL: return "destruction-physique";
        default: return "inconnue";
    }
}

const char *dfx_native_action_name(dfx_native_action action) {
    switch (action) {
        case DFX_NATIVE_NVME_BLOCK: return "nvme-block-erase";
        case DFX_NATIVE_NVME_CRYPTO: return "nvme-crypto-erase";
        default: return "aucune";
    }
}

const char *dfx_native_status_source_name(dfx_native_status_source source) {
    switch (source) {
        case DFX_NATIVE_STATUS_COMMAND: return "nvme-commande";
        case DFX_NATIVE_STATUS_SANITIZE_LOG: return "nvme-journal-sanitize";
        default: return "aucune";
    }
}

void dfx_format_size(uint64_t bytes, char *buffer, size_t size) {
    static const char *units[] = {"o", "Kio", "Mio", "Gio", "Tio", "Pio"};
    double value = (double)bytes;
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        unit++;
    }
    snprintf(buffer, size, "%.2f %s", value, units[unit]);
}

int dfx_validate_job(const dfx_job *job, char error[DFX_ERROR_MAX]) {
    if (job == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Tâche absente.");
        return -1;
    }
    if (job->method == DFX_METHOD_DESTROY_PHYSICAL) {
        snprintf(error, DFX_ERROR_MAX, "La destruction physique exige une station certifiée et ne peut pas être exécutée par logiciel.");
        return -1;
    }
    if (job->device.system_disk) {
        snprintf(error, DFX_ERROR_MAX, "Refus absolu : le périphérique contient le système en cours d’exécution.");
        return -1;
    }
    if (!job->device.regular_file && !job->device.safety_state_known) {
        snprintf(error, DFX_ERROR_MAX, "Refus : l’état de montage, d’utilisation ou de lecture seule n’a pas pu être établi sans ambiguïté.");
        return -1;
    }
    if (job->execute && job->device.internal && !job->allow_internal) {
        snprintf(error, DFX_ERROR_MAX, "Refus : un disque interne exige --allow-internal depuis un environnement de maintenance indépendant.");
        return -1;
    }
    if (job->device.mounted) {
        snprintf(error, DFX_ERROR_MAX, "Refus : une partition du périphérique est montée.");
        return -1;
    }
    if (job->device.in_use) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le périphérique possède un détenteur actif, un swap, un RAID ou une couche de volumes.");
        return -1;
    }
    if (job->device.read_only) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le périphérique est en lecture seule.");
        return -1;
    }
    if (!job->device.regular_file && !job->device.whole_device) {
        snprintf(error, DFX_ERROR_MAX, "Refus : la cible est une partition et non un disque physique entier.");
        return -1;
    }
    if (!job->device.regular_file && (job->device.size_bytes == 0 || job->device.logical_block_size == 0 || job->device.size_bytes % job->device.logical_block_size != 0)) {
        snprintf(error, DFX_ERROR_MAX, "Refus : la capacité ou la taille de bloc physique est nulle ou incohérente.");
        return -1;
    }
    if (!job->device.regular_file && !job->device.stable_identity_unique) {
        snprintf(error, DFX_ERROR_MAX, "Refus : aucun identifiant matériel unique et fiable n’est disponible pour cette cible.");
        return -1;
    }
    if (job->lab_mode && !dfx_lab_mode_available()) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le mode laboratoire n’est pas disponible dans ce binaire.");
        return -1;
    }
    if (!job->device.regular_file && !job->device.qualified && !job->lab_mode) {
        snprintf(error, DFX_ERROR_MAX, "Refus : ce tuple matériel, firmware, transport et système n’est pas qualifié pour cette version.");
        return -1;
    }
    if (job->method == DFX_METHOD_AUTO) {
        snprintf(error, DFX_ERROR_MAX, "La méthode automatique doit être résolue avant validation.");
        return -1;
    }
    if (job->device.kind == DFX_MEDIA_UNKNOWN) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le type de support n’a pas pu être déterminé de façon fiable.");
        return -1;
    }
    if (job->method == DFX_METHOD_CLEAR_ZERO && (job->device.kind == DFX_MEDIA_SSD || job->device.kind == DFX_MEDIA_NVME || job->device.kind == DFX_MEDIA_FLASH)) {
        snprintf(error, DFX_ERROR_MAX, "Refus : clear-zero ne garantit pas les blocs remappés d’un support flash ; utilisez purge-native lorsqu’elle est prise en charge.");
        return -1;
    }
    if (!job->device.regular_file && job->method == DFX_METHOD_CLEAR_ZERO && !job->lab_mode) {
        snprintf(error, DFX_ERROR_MAX, "Refus : clear-zero physique reste désactivé tant que l’identité matérielle ne peut pas être relue depuis le descripteur destructif ; utilisez uniquement un build laboratoire sur support sacrifiable.");
        return -1;
    }
    if (job->method == DFX_METHOD_PURGE_NATIVE && (job->device.kind != DFX_MEDIA_NVME || job->native_action == DFX_NATIVE_NONE)) {
        snprintf(error, DFX_ERROR_MAX, "Refus : la purge native résolue exige un NVMe direct et une sous-méthode explicite.");
        return -1;
    }
    if (job->method == DFX_METHOD_PURGE_NATIVE && job->verification_explicit) {
        snprintf(error, DFX_ERROR_MAX, "Refus : --verify full ou sample ne décrit pas la preuve d’une purge native ; la vérification repose sur le contrôleur et la qualification externe.");
        return -1;
    }
    if (job->execute && strcmp(job->confirmation, job->device.stable_id) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Confirmation invalide : recopiez exactement l’identifiant stable du périphérique.");
        return -1;
    }
    if (job->execute && !job->device.regular_file && !job->acknowledged) {
        snprintf(error, DFX_ERROR_MAX, "Confirmation incomplète : ajoutez --acknowledge-data-loss pour un disque physique.");
        return -1;
    }
    if (job->execute && !job->device.regular_file && job->audit_path[0] == '\0') {
        snprintf(error, DFX_ERROR_MAX, "Refus : --audit est obligatoire pour un disque physique.");
        return -1;
    }
    if (job->execute && !job->device.regular_file && job->operator_id[0] == '\0') {
        snprintf(error, DFX_ERROR_MAX, "Refus : --operator est obligatoire pour un disque physique.");
        return -1;
    }
    if (job->execute && !job->device.regular_file && job->witness_id[0] == '\0') {
        snprintf(error, DFX_ERROR_MAX, "Refus : --witness est obligatoire pour un disque physique.");
        return -1;
    }
    if (job->execute && !job->device.regular_file && strcmp(job->operator_id, job->witness_id) == 0) {
        snprintf(error, DFX_ERROR_MAX, "Refus : l’opérateur et le témoin doivent être deux identités distinctes.");
        return -1;
    }
    return 0;
}

int dfx_resolve_method(dfx_job *job, char error[DFX_ERROR_MAX]) {
    job->native_action = DFX_NATIVE_NONE;
    if (job->method == DFX_METHOD_AUTO) {
        if (job->device.kind == DFX_MEDIA_HDD || job->device.kind == DFX_MEDIA_FILE) job->method = DFX_METHOD_CLEAR_ZERO;
        else if (dfx_native_purge_action(&job->device, &job->native_action, error) == 0) job->method = DFX_METHOD_PURGE_NATIVE;
        else return -1;
    }
    if (job->method == DFX_METHOD_PURGE_NATIVE && dfx_native_purge_action(&job->device, &job->native_action, error) != 0) return -1;
    dfx_apply_qualification(job);
    return 0;
}
