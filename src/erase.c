#if !defined(__APPLE__) && !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "dfx.h"
#include "dfx_sha256.h"
#include "erase_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
typedef __int64 dfx_offset_t;
#define dfx_fileno _fileno
#define dfx_fsync _commit
#define dfx_seek _fseeki64
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/disk.h>
#else
#include <linux/fs.h>
#endif
typedef off_t dfx_offset_t;
#define dfx_fileno fileno
#define dfx_fsync fsync
#define dfx_seek fseeko
#endif

static FILE *open_target(const dfx_device *device, bool writable) {
#ifdef _WIN32
    DWORD access = GENERIC_READ | (writable ? GENERIC_WRITE : 0U);
    HANDLE handle = CreateFileA(device->path, access, 0, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EACCES;
        return NULL;
    }
    int descriptor = _open_osfhandle((intptr_t)handle, _O_BINARY | (writable ? _O_RDWR : _O_RDONLY));
    if (descriptor < 0) {
        CloseHandle(handle);
        return NULL;
    }
    FILE *file = _fdopen(descriptor, writable ? "r+b" : "rb");
    if (file == NULL) _close(descriptor);
    return file;
#else
    int flags = (writable ? O_RDWR : O_RDONLY) | O_CLOEXEC | O_NOFOLLOW;
#ifdef __linux__
    if (writable && !device->regular_file) flags |= O_EXCL;
#endif
    int descriptor = open(device->path, flags);
    if (descriptor < 0) return NULL;
    FILE *file = fdopen(descriptor, writable ? "r+b" : "rb");
    if (file == NULL) close(descriptor);
    return file;
#endif
}

static int validate_regular_content(FILE *file, const dfx_device *device, char error[DFX_ERROR_MAX]) {
    size_t identity_length = strlen(device->stable_id);
    if (identity_length < 65 || device->stable_id[identity_length - 65] != '-') {
        snprintf(error, DFX_ERROR_MAX, "Refus : l’empreinte de la cible confirmée est absente.");
        return -1;
    }
    if (dfx_seek(file, 0, SEEK_SET) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Positionnement du descripteur confirmé impossible : %s", strerror(errno));
        return -1;
    }
    unsigned char buffer[64U * 1024U];
    dfx_sha256_context context;
    dfx_sha256_init(&context);
    clearerr(file);
    for (;;) {
        size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count > 0) dfx_sha256_update(&context, buffer, count);
        if (count < sizeof(buffer)) {
            if (ferror(file)) {
                snprintf(error, DFX_ERROR_MAX, "Lecture de l’empreinte du descripteur confirmé impossible : %s", strerror(errno));
                return -1;
            }
            break;
        }
    }
    char hash[65];
    dfx_sha256_final_hex(&context, hash);
    if (dfx_seek(file, 0, SEEK_SET) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Repositionnement du descripteur confirmé impossible : %s", strerror(errno));
        return -1;
    }
    if (strcmp(hash, device->stable_id + identity_length - 64) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le contenu du descripteur ouvert ne correspond pas à la cible confirmée.");
        return -1;
    }
    return 0;
}

int dfx_validate_open_target(FILE *file, const dfx_job *job, char error[DFX_ERROR_MAX]) {
#ifdef _WIN32
    intptr_t raw_handle = _get_osfhandle(dfx_fileno(file));
    if (raw_handle == -1) {
        snprintf(error, DFX_ERROR_MAX, "Validation du descripteur Windows impossible.");
        return -1;
    }
    if (!job->device.regular_file) {
        snprintf(error, DFX_ERROR_MAX, "Refus : l’identité d’un disque physique Windows ne peut pas encore être revalidée depuis le descripteur ouvert.");
        return -1;
    }
    BY_HANDLE_FILE_INFORMATION information;
    LARGE_INTEGER size;
    if (!GetFileInformationByHandle((HANDLE)raw_handle, &information) || !GetFileSizeEx((HANDLE)raw_handle, &size)) {
        snprintf(error, DFX_ERROR_MAX, "Validation du fichier Windows ouvert impossible, code %lu.", GetLastError());
        return -1;
    }
    uint64_t file_index = ((uint64_t)information.nFileIndexHigh << 32) | information.nFileIndexLow;
    if (information.dwVolumeSerialNumber != job->device.object_identity_a || file_index != job->device.object_identity_b || (uint64_t)size.QuadPart != job->device.size_bytes) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le descripteur ouvert ne correspond pas au fichier confirmé.");
        return -1;
    }
#else
    struct stat opened_status;
    struct stat path_status;
    if (fstat(dfx_fileno(file), &opened_status) != 0 || lstat(job->device.path, &path_status) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Validation du descripteur ouvert impossible : %s", strerror(errno));
        return -1;
    }
    bool same_object;
    if (job->device.regular_file) {
        same_object = S_ISREG(opened_status.st_mode)
            && opened_status.st_dev == path_status.st_dev
            && opened_status.st_ino == path_status.st_ino
            && (uint64_t)opened_status.st_dev == job->device.object_identity_a
            && (uint64_t)opened_status.st_ino == job->device.object_identity_b
            && opened_status.st_size >= 0
            && (uint64_t)opened_status.st_size == job->device.size_bytes;
    } else {
        same_object = !S_ISREG(opened_status.st_mode)
            && opened_status.st_rdev == path_status.st_rdev
            && (uint64_t)opened_status.st_rdev == job->device.object_identity_a;
#ifdef __APPLE__
        uint32_t block_size = 0;
        uint64_t block_count = 0;
        if (same_object && (ioctl(dfx_fileno(file), DKIOCGETBLOCKSIZE, &block_size) != 0 || ioctl(dfx_fileno(file), DKIOCGETBLOCKCOUNT, &block_count) != 0)) {
            snprintf(error, DFX_ERROR_MAX, "Validation de la géométrie du descripteur ouvert impossible : %s", strerror(errno));
            return -1;
        }
        same_object = same_object
            && block_size != 0
            && block_count <= UINT64_MAX / block_size
            && block_count * block_size == job->device.size_bytes
            && block_size == job->device.logical_block_size;
#else
        uint64_t size_bytes = 0;
        uint32_t block_size = 0;
        if (same_object && !S_ISBLK(opened_status.st_mode)) {
            snprintf(error, DFX_ERROR_MAX, "Refus : le descripteur ouvert n’est pas un périphérique bloc Linux.");
            return -1;
        }
        if (same_object && (ioctl(dfx_fileno(file), BLKGETSIZE64, &size_bytes) != 0 || ioctl(dfx_fileno(file), BLKSSZGET, &block_size) != 0)) {
            snprintf(error, DFX_ERROR_MAX, "Validation de la géométrie du descripteur ouvert impossible : %s", strerror(errno));
            return -1;
        }
        same_object = same_object
            && size_bytes == job->device.size_bytes
            && block_size == job->device.logical_block_size;
#endif
    }
    if (!same_object) {
        snprintf(error, DFX_ERROR_MAX, "Refus : le descripteur ouvert ne correspond pas à la cible confirmée.");
        return -1;
    }
#endif
    if (job->device.regular_file && validate_regular_content(file, &job->device, error) != 0) return -1;
    return 0;
}

static int prepare_verification(FILE *file, char error[DFX_ERROR_MAX]) {
#ifndef _WIN32
    int descriptor = dfx_fileno(file);
#ifdef __APPLE__
    if (fcntl(descriptor, F_NOCACHE, 1) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Désactivation du cache avant vérification impossible : %s", strerror(errno));
        return -1;
    }
#elif defined(POSIX_FADV_DONTNEED)
    int result = posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
    if (result != 0) {
        snprintf(error, DFX_ERROR_MAX, "Invalidation du cache avant vérification impossible : %s", strerror(result));
        return -1;
    }
#endif
#else
    (void)file;
    (void)error;
#endif
    return 0;
}

static int verify_zero(FILE *file, uint64_t size, bool full, char error[DFX_ERROR_MAX]) {
    const size_t chunk_size = 1024U * 1024U;
    unsigned char *buffer = malloc(chunk_size);
    if (buffer == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Mémoire insuffisante pendant la vérification.");
        return -1;
    }
    uint64_t positions[3] = {0, size / 2, size > chunk_size ? size - chunk_size : 0};
    uint64_t full_rounds = size / chunk_size + (size % chunk_size != 0 ? 1U : 0U);
    if (full && full_rounds > SIZE_MAX) {
        free(buffer);
        snprintf(error, DFX_ERROR_MAX, "Taille trop grande pour la vérification sur cette architecture.");
        return -1;
    }
    size_t rounds = full ? (size_t)full_rounds : 3;
    for (size_t index = 0; index < rounds; index++) {
        uint64_t position = full ? (uint64_t)index * chunk_size : positions[index];
        if (position >= size) continue;
        size_t wanted = (size - position < chunk_size) ? (size_t)(size - position) : chunk_size;
        if (dfx_seek(file, (dfx_offset_t)position, SEEK_SET) != 0 || fread(buffer, 1, wanted, file) != wanted) {
            free(buffer);
            snprintf(error, DFX_ERROR_MAX, "Lecture de vérification impossible : %s", strerror(errno));
            return -1;
        }
        for (size_t byte = 0; byte < wanted; byte++) {
            if (buffer[byte] != 0) {
                free(buffer);
                snprintf(error, DFX_ERROR_MAX, "Vérification négative à l’octet %llu.", (unsigned long long)(position + byte));
                return -1;
            }
        }
    }
    free(buffer);
    return 0;
}

int dfx_run_job(dfx_job *job, dfx_progress_fn progress, void *context, char error[DFX_ERROR_MAX]) {
    if (dfx_validate_job(job, error) != 0) return -1;
    if (!job->execute) return 0;
    dfx_device current;
    if (dfx_inspect_device(job->device.path, &current, error) != 0) return -1;
    if (strcmp(current.stable_id, job->device.stable_id) != 0 || current.size_bytes != job->device.size_bytes || current.logical_block_size != job->device.logical_block_size || current.kind != job->device.kind || current.object_identity_a != job->device.object_identity_a || current.object_identity_b != job->device.object_identity_b || current.stable_identity_unique != job->device.stable_identity_unique || strcmp(current.model, job->device.model) != 0 || strcmp(current.firmware, job->device.firmware) != 0 || strcmp(current.transport, job->device.transport) != 0 || strcmp(current.environment, job->device.environment) != 0 || strcmp(current.topology, job->device.topology) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Refus : l’identité du périphérique a changé depuis la confirmation.");
        return -1;
    }
    dfx_job revalidated = *job;
    revalidated.device = current;
    dfx_apply_qualification(&revalidated);
    if (dfx_validate_job(&revalidated, error) != 0) return -1;
    if (job->method == DFX_METHOD_PURGE_NATIVE) {
        int result = dfx_run_native_purge(&revalidated, progress, context, error);
        job->native_status_raw = revalidated.native_status_raw;
        job->native_status_observed = revalidated.native_status_observed;
        job->native_status_source = revalidated.native_status_source;
        return result;
    }
    FILE *file = open_target(&revalidated.device, true);
    if (file == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Ouverture en écriture impossible : %s", strerror(errno));
        return -1;
    }
    if (dfx_validate_open_target(file, &revalidated, error) != 0) {
        fclose(file);
        return -1;
    }
    const size_t chunk_size = 8U * 1024U * 1024U;
    unsigned char *zeros = calloc(1, chunk_size);
    if (zeros == NULL) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Mémoire insuffisante.");
        return -1;
    }
    uint64_t written = 0;
    while (written < job->device.size_bytes) {
        if (dfx_cancel_requested()) {
            free(zeros);
            fflush(file);
            dfx_fsync(dfx_fileno(file));
            fclose(file);
            snprintf(error, DFX_ERROR_MAX, "Effacement interrompu à %llu octets ; le support est dans un état partiellement effacé.", (unsigned long long)written);
            return -1;
        }
        size_t wanted = (job->device.size_bytes - written < chunk_size) ? (size_t)(job->device.size_bytes - written) : chunk_size;
        if (fwrite(zeros, 1, wanted, file) != wanted) {
            free(zeros);
            fclose(file);
            snprintf(error, DFX_ERROR_MAX, "Écriture interrompue après %llu octets : %s", (unsigned long long)written, strerror(errno));
            return -1;
        }
        written += wanted;
        if (progress != NULL) progress(written, job->device.size_bytes, context);
    }
    free(zeros);
    if (fflush(file) != 0 || dfx_fsync(dfx_fileno(file)) != 0) {
        fclose(file);
        snprintf(error, DFX_ERROR_MAX, "Synchronisation physique impossible : %s", strerror(errno));
        return -1;
    }
    if (prepare_verification(file, error) != 0) {
        fclose(file);
        return -1;
    }
    if (verify_zero(file, job->device.size_bytes, job->full_verify, error) != 0) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Fermeture du périphérique impossible : %s", strerror(errno));
        return -1;
    }
    return 0;
}
