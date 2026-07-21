#if !defined(__APPLE__) && !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "dfx.h"
#include "dfx_sha256.h"

#include <errno.h>
#include <string.h>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

static bool valid_device_path(const char *path) {
    return path != NULL && path[0] != '\0' && strlen(path) < DFX_PATH_MAX && dfx_text_is_safe(path);
}

static bool display_text_safe(const char *text) {
    return dfx_text_is_safe(text);
}

static void set_environment(dfx_device *device) {
#ifdef _WIN32
    snprintf(device->environment, sizeof(device->environment), "Windows");
#else
    struct utsname information;
    if (uname(&information) == 0) snprintf(device->environment, sizeof(device->environment), "%.31s %.63s %.31s", information.sysname, information.release, information.machine);
    else snprintf(device->environment, sizeof(device->environment), "Non déterminé");
#endif
    if (!display_text_safe(device->environment)) snprintf(device->environment, sizeof(device->environment), "Non déterminé");
    snprintf(device->topology, sizeof(device->topology), "inconnue");
}

static int hash_stream(FILE *file, char hash[65], char error[DFX_ERROR_MAX]) {
    unsigned char buffer[64U * 1024U];
    dfx_sha256_context context;
    dfx_sha256_init(&context);
    for (;;) {
        size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count > 0) dfx_sha256_update(&context, buffer, count);
        if (count < sizeof(buffer)) {
            if (ferror(file)) {
                snprintf(error, DFX_ERROR_MAX, "Lecture de l’empreinte du fichier impossible : %s", strerror(errno));
                return -1;
            }
            break;
        }
    }
    dfx_sha256_final_hex(&context, hash);
    return 0;
}

#ifndef _WIN32
static int hash_regular_file(const char *path, const struct stat *expected, char hash[65], char error[DFX_ERROR_MAX]) {
    int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        snprintf(error, DFX_ERROR_MAX, "Ouverture du fichier pour son empreinte impossible : %s", strerror(errno));
        return -1;
    }
    struct stat opened;
    if (fstat(descriptor, &opened) != 0 || !S_ISREG(opened.st_mode) || opened.st_dev != expected->st_dev || opened.st_ino != expected->st_ino || opened.st_size != expected->st_size) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Le fichier a changé avant le calcul de son identité.");
        return -1;
    }
    FILE *file = fdopen(descriptor, "rb");
    if (file == NULL) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Ouverture du flux pour l’empreinte impossible : %s", strerror(errno));
        return -1;
    }
    int result = hash_stream(file, hash, error);
    struct stat final_status;
    if (result == 0 && (fstat(descriptor, &final_status) != 0 || final_status.st_size != opened.st_size
#ifdef __APPLE__
        || final_status.st_mtimespec.tv_sec != opened.st_mtimespec.tv_sec || final_status.st_mtimespec.tv_nsec != opened.st_mtimespec.tv_nsec || final_status.st_ctimespec.tv_sec != opened.st_ctimespec.tv_sec || final_status.st_ctimespec.tv_nsec != opened.st_ctimespec.tv_nsec
#else
        || final_status.st_mtim.tv_sec != opened.st_mtim.tv_sec || final_status.st_mtim.tv_nsec != opened.st_mtim.tv_nsec || final_status.st_ctim.tv_sec != opened.st_ctim.tv_sec || final_status.st_ctim.tv_nsec != opened.st_ctim.tv_nsec
#endif
    )) {
        snprintf(error, DFX_ERROR_MAX, "Le fichier a changé pendant le calcul de son identité.");
        result = -1;
    }
    if (fclose(file) != 0 && result == 0) {
        snprintf(error, DFX_ERROR_MAX, "Fermeture du fichier après son empreinte impossible : %s", strerror(errno));
        return -1;
    }
    return result;
}
#endif

#ifdef _WIN32

#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <winioctl.h>

static int hash_windows_file(HANDLE handle, char hash[65], char error[DFX_ERROR_MAX]) {
    HANDLE duplicate = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(), &duplicate, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        snprintf(error, DFX_ERROR_MAX, "Duplication du fichier pour son empreinte impossible, code %lu.", GetLastError());
        return -1;
    }
    LARGE_INTEGER beginning;
    beginning.QuadPart = 0;
    if (!SetFilePointerEx(duplicate, beginning, NULL, FILE_BEGIN)) {
        CloseHandle(duplicate);
        snprintf(error, DFX_ERROR_MAX, "Positionnement du fichier pour son empreinte impossible, code %lu.", GetLastError());
        return -1;
    }
    int descriptor = _open_osfhandle((intptr_t)duplicate, _O_RDONLY | _O_BINARY);
    if (descriptor < 0) {
        CloseHandle(duplicate);
        snprintf(error, DFX_ERROR_MAX, "Conversion du fichier pour son empreinte impossible.");
        return -1;
    }
    FILE *file = _fdopen(descriptor, "rb");
    if (file == NULL) {
        _close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Ouverture du fichier pour son empreinte impossible.");
        return -1;
    }
    int result = hash_stream(file, hash, error);
    if (fclose(file) != 0 && result == 0) {
        snprintf(error, DFX_ERROR_MAX, "Fermeture du fichier après son empreinte impossible.");
        return -1;
    }
    return result;
}

static uint64_t file_size(HANDLE handle) {
    LARGE_INTEGER size;
    return GetFileSizeEx(handle, &size) ? (uint64_t)size.QuadPart : 0;
}

static bool storage_text(const unsigned char *buffer, DWORD returned, DWORD offset, char *output, size_t size) {
    if (offset == 0 || offset >= returned || size == 0) return false;
    const char *start = (const char *)buffer + offset;
    size_t available = (size_t)returned - offset;
    const char *end = memchr(start, '\0', available);
    if (end == NULL) return false;
    size_t length = (size_t)(end - start);
    while (length > 0 && start[length - 1] == ' ') length--;
    if (length >= size) return false;
    for (size_t index = 0; index < length; index++) {
        unsigned char value = (unsigned char)start[index];
        if (value < 0x20U || value == 0x7fU) return false;
    }
    memcpy(output, start, length);
    output[length] = '\0';
    if (display_text_safe(output)) return true;
    output[0] = '\0';
    return false;
}

int dfx_inspect_device(const char *path, dfx_device *device, char error[DFX_ERROR_MAX]) {
    memset(device, 0, sizeof(*device));
    set_environment(device);
    if (!valid_device_path(path)) {
        snprintf(error, DFX_ERROR_MAX, "Chemin de périphérique absent, trop long, contenant un caractère de contrôle ou un UTF-8 invalide.");
        return -1;
    }
    snprintf(device->path, sizeof(device->path), "%s", path);
    snprintf(device->stable_id, sizeof(device->stable_id), "%s", path);
    snprintf(device->model, sizeof(device->model), "Non déterminé");
    snprintf(device->firmware, sizeof(device->firmware), "Non déterminé");
    snprintf(device->transport, sizeof(device->transport), "inconnu");
    DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Refus : les points de réanalyse Windows ne sont pas acceptés.");
        return -1;
    }
    bool regular_candidate = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    HANDLE handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        snprintf(error, DFX_ERROR_MAX, "Ouverture impossible, code Windows %lu.", GetLastError());
        return -1;
    }
    DISK_GEOMETRY_EX geometry;
    DWORD returned = 0;
    if (DeviceIoControl(handle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geometry, (DWORD)sizeof(geometry), &returned, NULL)) {
        if (geometry.DiskSize.QuadPart < 0 || geometry.Geometry.BytesPerSector == 0) {
            CloseHandle(handle);
            snprintf(error, DFX_ERROR_MAX, "Géométrie du disque invalide.");
            return -1;
        }
        device->size_bytes = (uint64_t)geometry.DiskSize.QuadPart;
        device->logical_block_size = geometry.Geometry.BytesPerSector;
        device->kind = geometry.Geometry.MediaType == FixedMedia ? DFX_MEDIA_UNKNOWN : DFX_MEDIA_FLASH;
        device->removable = geometry.Geometry.MediaType == RemovableMedia;
        device->internal = !device->removable;
        unsigned char property_buffer[1024];
        memset(property_buffer, 0, sizeof(property_buffer));
        STORAGE_PROPERTY_QUERY property_query;
        memset(&property_query, 0, sizeof(property_query));
        property_query.PropertyId = StorageDeviceProperty;
        property_query.QueryType = PropertyStandardQuery;
        DWORD property_returned = 0;
        if (DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &property_query, (DWORD)sizeof(property_query), property_buffer, (DWORD)sizeof(property_buffer), &property_returned, NULL) && property_returned >= offsetof(STORAGE_DEVICE_DESCRIPTOR, RawDeviceProperties)) {
            STORAGE_DEVICE_DESCRIPTOR *descriptor = (STORAGE_DEVICE_DESCRIPTOR *)property_buffer;
            if (!storage_text(property_buffer, property_returned, descriptor->ProductIdOffset, device->model, sizeof(device->model))) snprintf(device->model, sizeof(device->model), "Non déterminé");
            if (!storage_text(property_buffer, property_returned, descriptor->ProductRevisionOffset, device->firmware, sizeof(device->firmware))) snprintf(device->firmware, sizeof(device->firmware), "Non déterminé");
            char serial[DFX_ID_MAX] = {0};
            if (storage_text(property_buffer, property_returned, descriptor->SerialNumberOffset, serial, sizeof(serial)) && dfx_hardware_id_is_credible(serial)) {
                snprintf(device->stable_id, sizeof(device->stable_id), "windows-%.180s-%llu", serial, (unsigned long long)device->size_bytes);
                device->stable_identity_unique = true;
            }
            if (descriptor->BusType == BusTypeNvme) {
                device->kind = DFX_MEDIA_NVME;
                snprintf(device->transport, sizeof(device->transport), "NVMe");
            } else if (descriptor->BusType == BusTypeUsb) {
                device->internal = false;
                snprintf(device->transport, sizeof(device->transport), "USB");
            } else if (descriptor->BusType == BusTypeAta || descriptor->BusType == BusTypeSata) snprintf(device->transport, sizeof(device->transport), "SATA");
            else if (descriptor->BusType == BusTypeScsi) snprintf(device->transport, sizeof(device->transport), "SCSI");
        }
        if (device->kind != DFX_MEDIA_NVME) {
            STORAGE_PROPERTY_QUERY seek_query;
            memset(&seek_query, 0, sizeof(seek_query));
            seek_query.PropertyId = StorageDeviceSeekPenaltyProperty;
            seek_query.QueryType = PropertyStandardQuery;
            DEVICE_SEEK_PENALTY_DESCRIPTOR seek_penalty;
            DWORD seek_returned = 0;
            if (DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY, &seek_query, (DWORD)sizeof(seek_query), &seek_penalty, (DWORD)sizeof(seek_penalty), &seek_returned, NULL)) device->kind = seek_penalty.IncursSeekPenalty ? DFX_MEDIA_HDD : DFX_MEDIA_SSD;
        }
        STORAGE_DEVICE_NUMBER number;
        DWORD number_returned = 0;
        if (DeviceIoControl(handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &number, (DWORD)sizeof(number), &number_returned, NULL)) {
            device->whole_device = number.DeviceType == FILE_DEVICE_DISK && number.PartitionNumber == (DWORD)-1;
            device->system_disk = device->whole_device && number.DeviceNumber == 0;
            device->object_identity_a = number.DeviceNumber;
            device->object_identity_b = number.PartitionNumber;
            snprintf(device->topology, sizeof(device->topology), "disque-%lu-%.80s-%.120s", number.DeviceNumber, device->transport, device->model);
        }
    } else if (regular_candidate) {
        device->size_bytes = file_size(handle);
        device->logical_block_size = 1;
        device->kind = DFX_MEDIA_FILE;
        device->regular_file = true;
        device->whole_device = true;
        device->safety_state_known = true;
        device->stable_identity_unique = true;
        device->read_only = (attributes & FILE_ATTRIBUTE_READONLY) != 0;
        BY_HANDLE_FILE_INFORMATION information;
        if (!GetFileInformationByHandle(handle, &information)) {
            DWORD code = GetLastError();
            CloseHandle(handle);
            snprintf(error, DFX_ERROR_MAX, "Identité du fichier inaccessible, code Windows %lu.", code);
            return -1;
        }
        char content_hash[65];
        if (hash_windows_file(handle, content_hash, error) != 0) {
            CloseHandle(handle);
            return -1;
        }
        BY_HANDLE_FILE_INFORMATION final_information;
        if (!GetFileInformationByHandle(handle, &final_information) || information.nFileSizeHigh != final_information.nFileSizeHigh || information.nFileSizeLow != final_information.nFileSizeLow || information.ftLastWriteTime.dwHighDateTime != final_information.ftLastWriteTime.dwHighDateTime || information.ftLastWriteTime.dwLowDateTime != final_information.ftLastWriteTime.dwLowDateTime) {
            CloseHandle(handle);
            snprintf(error, DFX_ERROR_MAX, "Le fichier a changé pendant le calcul de son identité.");
            return -1;
        }
        snprintf(device->stable_id, sizeof(device->stable_id), "fichier-%lu-%lu-%lu-%.64s", information.dwVolumeSerialNumber, information.nFileIndexHigh, information.nFileIndexLow, content_hash);
        device->object_identity_a = information.dwVolumeSerialNumber;
        device->object_identity_b = ((uint64_t)information.nFileIndexHigh << 32) | information.nFileIndexLow;
        snprintf(device->model, sizeof(device->model), "Fichier de test");
        snprintf(device->firmware, sizeof(device->firmware), "n/a");
        snprintf(device->transport, sizeof(device->transport), "virtuel");
        snprintf(device->topology, sizeof(device->topology), "fichier");
    } else {
        DWORD code = GetLastError();
        CloseHandle(handle);
        snprintf(error, DFX_ERROR_MAX, "Géométrie du disque inaccessible, code Windows %lu.", code);
        return -1;
    }
    CloseHandle(handle);
    return 0;
}

int dfx_list_devices(FILE *out, char error[DFX_ERROR_MAX]) {
    unsigned found = 0;
    for (unsigned index = 0; index < 64; index++) {
        char path[64];
        snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%u", index);
        dfx_device device;
        if (dfx_inspect_device(path, &device, error) == 0 && !device.regular_file) {
            char size[64];
            dfx_format_size(device.size_bytes, size, sizeof(size));
            fprintf(out, "%s\t%s\t%s\n", device.path, size, dfx_media_name(device.kind));
            found++;
        }
    }
    if (found == 0) fprintf(out, "Aucun disque accessible. Relancez dans un terminal administrateur.\n");
    error[0] = '\0';
    return 0;
}

#elif defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <fcntl.h>
#include <sys/disk.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static CFTypeRef registry_property(io_registry_entry_t service, CFStringRef key) {
    return IORegistryEntrySearchCFProperty(service, kIOServicePlane, key, kCFAllocatorDefault, kIORegistryIterateRecursively | kIORegistryIterateParents);
}

static bool registry_boolean(io_registry_entry_t service, CFStringRef key, bool fallback) {
    CFTypeRef value = registry_property(service, key);
    if (value == NULL) return fallback;
    bool result = fallback;
    if (CFGetTypeID(value) == CFBooleanGetTypeID()) result = CFBooleanGetValue((CFBooleanRef)value);
    CFRelease(value);
    return result;
}

static bool registry_boolean_value(io_registry_entry_t service, CFStringRef key, bool *output) {
    CFTypeRef value = registry_property(service, key);
    if (value == NULL) return false;
    bool valid = CFGetTypeID(value) == CFBooleanGetTypeID();
    if (valid) *output = CFBooleanGetValue((CFBooleanRef)value);
    CFRelease(value);
    return valid;
}

static void registry_string(io_registry_entry_t service, CFStringRef key, char *output, size_t size) {
    CFTypeRef value = registry_property(service, key);
    if (value == NULL) return;
    if (CFGetTypeID(value) == CFStringGetTypeID()) CFStringGetCString((CFStringRef)value, output, (CFIndex)size, kCFStringEncodingUTF8);
    else if (CFGetTypeID(value) == CFDataGetTypeID()) {
        CFDataRef data = (CFDataRef)value;
        CFIndex length = CFDataGetLength(data);
        if ((size_t)length >= size) length = (CFIndex)size - 1;
        memcpy(output, CFDataGetBytePtr(data), (size_t)length);
        output[length] = '\0';
        while (length > 0 && (output[length - 1] == ' ' || output[length - 1] == '\0')) output[--length] = '\0';
    }
    CFRelease(value);
}

static void registry_details(const char *path, dfx_device *device) {
    const char *name = strrchr(path, '/');
    name = name == NULL ? path : name + 1;
    if (name[0] == 'r' && strncmp(name, "rdisk", 5) == 0) name++;
    CFMutableDictionaryRef matching = IOBSDNameMatching(kIOMainPortDefault, 0, name);
    if (matching == NULL) return;
    io_registry_entry_t service = IOServiceGetMatchingService(kIOMainPortDefault, matching);
    if (service == IO_OBJECT_NULL) return;
    bool solid_state = false;
    bool medium_known = registry_boolean_value(service, CFSTR("Solid State"), &solid_state);
    device->removable = registry_boolean(service, CFSTR("Removable"), false);
    device->internal = registry_boolean(service, CFSTR("Internal"), true);
    registry_string(service, CFSTR("Model"), device->model, sizeof(device->model));
    registry_string(service, CFSTR("Revision"), device->firmware, sizeof(device->firmware));
    registry_string(service, CFSTR("Physical Interconnect"), device->transport, sizeof(device->transport));
    char serial[DFX_ID_MAX] = {0};
    registry_string(service, CFSTR("Serial Number"), serial, sizeof(serial));
    if (dfx_hardware_id_is_credible(serial)) {
        snprintf(device->stable_id, sizeof(device->stable_id), "macos-%s", serial);
        device->stable_identity_unique = true;
    }
    io_string_t registry_path;
    if (IORegistryEntryGetPath(service, kIOServicePlane, registry_path) == KERN_SUCCESS) snprintf(device->topology, sizeof(device->topology), "%.240s", registry_path);
    if (!display_text_safe(device->model)) snprintf(device->model, sizeof(device->model), "Non déterminé");
    if (!display_text_safe(device->firmware)) snprintf(device->firmware, sizeof(device->firmware), "Non déterminé");
    if (!display_text_safe(device->transport)) snprintf(device->transport, sizeof(device->transport), "inconnu");
    if (!display_text_safe(device->topology)) snprintf(device->topology, sizeof(device->topology), "inconnue");
    device->kind = medium_known ? (solid_state ? DFX_MEDIA_SSD : DFX_MEDIA_HDD) : DFX_MEDIA_UNKNOWN;
    if (strstr(device->model, "NVMe") != NULL) device->kind = DFX_MEDIA_NVME;
    IOObjectRelease(service);
}

static void canonical_device(const char *path, char output[DFX_PATH_MAX]) {
    if (strncmp(path, "/dev/rdisk", 10) == 0) snprintf(output, DFX_PATH_MAX, "/dev/disk%s", path + 10);
    else snprintf(output, DFX_PATH_MAX, "%s", path);
}

static bool whole_disk_path(const char *path) {
    const char *name = strrchr(path, '/');
    name = name == NULL ? path : name + 1;
    if (strncmp(name, "rdisk", 5) == 0) name += 5;
    else if (strncmp(name, "disk", 4) == 0) name += 4;
    else return false;
    if (*name < '0' || *name > '9') return false;
    while (*name >= '0' && *name <= '9') name++;
    return *name == '\0';
}

static bool device_contains(const char *whole, const char *candidate) {
    size_t length = strlen(whole);
    return strncmp(whole, candidate, length) == 0 && (candidate[length] == '\0' || candidate[length] == 's');
}

static void mount_state(const char *path, bool *mounted, bool *system_disk) {
    struct statfs *mounts = NULL;
    int count = getmntinfo(&mounts, MNT_NOWAIT);
    char whole[DFX_PATH_MAX];
    canonical_device(path, whole);
    for (int index = 0; index < count; index++) {
        char source[DFX_PATH_MAX];
        canonical_device(mounts[index].f_mntfromname, source);
        if (device_contains(whole, source)) {
            *mounted = true;
            if (strcmp(mounts[index].f_mntonname, "/") == 0) *system_disk = true;
        }
    }
}

int dfx_inspect_device(const char *path, dfx_device *device, char error[DFX_ERROR_MAX]) {
    memset(device, 0, sizeof(*device));
    set_environment(device);
    if (!valid_device_path(path)) {
        snprintf(error, DFX_ERROR_MAX, "Chemin de périphérique absent, trop long, contenant un caractère de contrôle ou un UTF-8 invalide.");
        return -1;
    }
    struct stat status;
    if (lstat(path, &status) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Périphérique introuvable : %s", strerror(errno));
        return -1;
    }
    if (S_ISLNK(status.st_mode)) {
        snprintf(error, DFX_ERROR_MAX, "Refus : les liens symboliques ne sont pas acceptés.");
        return -1;
    }
    snprintf(device->path, sizeof(device->path), "%s", path);
    snprintf(device->model, sizeof(device->model), "Non déterminé");
    snprintf(device->firmware, sizeof(device->firmware), "Non déterminé");
    snprintf(device->transport, sizeof(device->transport), "inconnu");
    if (S_ISREG(status.st_mode)) {
        device->regular_file = true;
        device->kind = DFX_MEDIA_FILE;
        device->size_bytes = (uint64_t)status.st_size;
        device->logical_block_size = 1;
        device->whole_device = true;
        device->safety_state_known = true;
        device->stable_identity_unique = true;
        snprintf(device->firmware, sizeof(device->firmware), "n/a");
        snprintf(device->transport, sizeof(device->transport), "virtuel");
        snprintf(device->topology, sizeof(device->topology), "fichier");
        char content_hash[65];
        if (hash_regular_file(path, &status, content_hash, error) != 0) return -1;
        snprintf(device->stable_id, sizeof(device->stable_id), "fichier-%llu-%llu-%.64s", (unsigned long long)status.st_dev, (unsigned long long)status.st_ino, content_hash);
        device->object_identity_a = (uint64_t)status.st_dev;
        device->object_identity_b = (uint64_t)status.st_ino;
        return 0;
    }
    int descriptor = open(path, O_RDONLY | O_NONBLOCK);
    if (descriptor < 0) {
        snprintf(error, DFX_ERROR_MAX, "Ouverture impossible : %s", strerror(errno));
        return -1;
    }
    uint32_t block_size = 0;
    uint64_t block_count = 0;
    if (ioctl(descriptor, DKIOCGETBLOCKSIZE, &block_size) != 0 || ioctl(descriptor, DKIOCGETBLOCKCOUNT, &block_count) != 0) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Géométrie du disque inaccessible : %s", strerror(errno));
        return -1;
    }
    int writable = 0;
    device->read_only = ioctl(descriptor, DKIOCISWRITABLE, &writable) != 0 || writable == 0;
    close(descriptor);
    if (block_size == 0 || block_count > UINT64_MAX / block_size) {
        snprintf(error, DFX_ERROR_MAX, "Géométrie du disque incohérente ou trop grande.");
        return -1;
    }
    device->size_bytes = block_count * block_size;
    device->object_identity_a = (uint64_t)status.st_rdev;
    device->logical_block_size = block_size;
    device->kind = DFX_MEDIA_UNKNOWN;
    device->whole_device = whole_disk_path(path);
    snprintf(device->stable_id, sizeof(device->stable_id), "macos-%llu-%llu-%u", (unsigned long long)status.st_rdev, (unsigned long long)block_count, block_size);
    registry_details(path, device);
    mount_state(path, &device->mounted, &device->system_disk);
    return 0;
}

int dfx_list_devices(FILE *out, char error[DFX_ERROR_MAX]) {
    unsigned found = 0;
    for (unsigned index = 0; index < 64; index++) {
        char path[64];
        snprintf(path, sizeof(path), "/dev/disk%u", index);
        dfx_device device;
        if (dfx_inspect_device(path, &device, error) == 0) {
            char size[64];
            dfx_format_size(device.size_bytes, size, sizeof(size));
            fprintf(out, "%s\t%s\t%s\t%s\n", device.path, size, dfx_media_name(device.kind), device.mounted ? "monté" : "non monté");
            found++;
        }
    }
    if (found == 0) fprintf(out, "Aucun disque accessible.\n");
    error[0] = '\0';
    return 0;
}

#else

#include <dirent.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int read_text(const char *path, char *buffer, size_t size) {
    FILE *file = fopen(path, "r");
    if (file == NULL) return -1;
    if (fgets(buffer, (int)size, file) == NULL) {
        fclose(file);
        return -1;
    }
    bool complete = strchr(buffer, '\n') != NULL || feof(file);
    if (!complete) {
        fclose(file);
        return -1;
    }
    fclose(file);
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return display_text_safe(buffer) ? 0 : -1;
}

static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static bool nvme_controller_name(const char *name, char *controller, size_t size) {
    if (strncmp(name, "nvme", 4) != 0) return false;
    const char *cursor = name + 4;
    if (*cursor < '0' || *cursor > '9') return false;
    while (*cursor >= '0' && *cursor <= '9') cursor++;
    if (*cursor != 'n') return false;
    size_t length = (size_t)(cursor - name);
    if (length >= size) return false;
    memcpy(controller, name, length);
    controller[length] = '\0';
    return true;
}

static int read_device_text(const char *name, const char *field, char *buffer, size_t size) {
    char path[DFX_PATH_MAX];
    snprintf(path, sizeof(path), "/sys/class/block/%s/device/%s", name, field);
    if (read_text(path, buffer, size) == 0) return 0;
    char controller[64];
    if (!nvme_controller_name(name, controller, sizeof(controller))) return -1;
    snprintf(path, sizeof(path), "/sys/class/nvme/%s/%s", controller, field);
    return read_text(path, buffer, size);
}

static bool same_block_family(const char *base, const char *candidate) {
    size_t length = strlen(base);
    return strncmp(candidate, base, length) == 0 && (candidate[length] == '\0' || candidate[length] == 'p' || (candidate[length] >= '0' && candidate[length] <= '9'));
}

static bool family_contains_device(const char *path, unsigned device_major, unsigned device_minor, bool *known) {
    struct stat target_status;
    if (stat(path, &target_status) != 0) {
        *known = false;
        return false;
    }
    if (major(target_status.st_rdev) == device_major && minor(target_status.st_rdev) == device_minor) return true;
    const char *name = base_name(path);
    DIR *directory = opendir("/sys/class/block");
    if (directory == NULL) {
        *known = false;
        return false;
    }
    bool found = false;
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) *known = false;
            break;
        }
        if (!same_block_family(name, entry->d_name)) continue;
        char dev_path[DFX_PATH_MAX];
        char value[64];
        snprintf(dev_path, sizeof(dev_path), "/sys/class/block/%s/dev", entry->d_name);
        unsigned entry_major = 0;
        unsigned entry_minor = 0;
        if (read_text(dev_path, value, sizeof(value)) != 0 || sscanf(value, "%u:%u", &entry_major, &entry_minor) != 2) {
            *known = false;
            continue;
        }
        if (entry_major == device_major && entry_minor == device_minor) found = true;
    }
    closedir(directory);
    return found;
}

static bool direct_mount_state(const char *path, bool *system_disk, bool *known) {
    FILE *file = fopen("/proc/self/mountinfo", "r");
    if (file == NULL) {
        *known = false;
        return false;
    }
    bool mounted = false;
    char line[4096];
    while (fgets(line, sizeof(line), file) != NULL) {
        char mountpoint[DFX_PATH_MAX] = {0};
        unsigned mount_major = 0;
        unsigned mount_minor = 0;
        if (sscanf(line, "%*u %*u %u:%u %*s %1023s", &mount_major, &mount_minor, mountpoint) != 3) {
            *known = false;
            continue;
        }
        if (family_contains_device(path, mount_major, mount_minor, known)) {
            mounted = true;
            if (strcmp(mountpoint, "/") == 0) *system_disk = true;
        }
    }
    if (ferror(file)) *known = false;
    fclose(file);
    return mounted;
}

static bool holders_mounted(const char *name, bool *system_disk, bool *in_use, bool *known, unsigned depth) {
    if (depth > 8) {
        *known = false;
        *in_use = true;
        return false;
    }
    char directory_path[DFX_PATH_MAX];
    snprintf(directory_path, sizeof(directory_path), "/sys/class/block/%s/holders", name);
    DIR *directory = opendir(directory_path);
    if (directory == NULL) {
        *known = false;
        return false;
    }
    bool mounted = false;
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) *known = false;
            break;
        }
        if (entry->d_name[0] == '.') continue;
        *in_use = true;
        char holder_path[DFX_PATH_MAX];
        snprintf(holder_path, sizeof(holder_path), "/dev/%s", entry->d_name);
        if (direct_mount_state(holder_path, system_disk, known)) mounted = true;
        if (holders_mounted(entry->d_name, system_disk, in_use, known, depth + 1)) mounted = true;
    }
    closedir(directory);
    return mounted;
}

static bool swap_state(const char *path, bool *known) {
    FILE *file = fopen("/proc/swaps", "r");
    if (file == NULL) {
        *known = false;
        return false;
    }
    bool active = false;
    char line[4096];
    if (fgets(line, sizeof(line), file) == NULL && ferror(file)) *known = false;
    while (fgets(line, sizeof(line), file) != NULL) {
        char source[DFX_PATH_MAX];
        if (sscanf(line, "%1023s", source) != 1) {
            *known = false;
            continue;
        }
        struct stat source_status;
        if (stat(source, &source_status) != 0) {
            *known = false;
            continue;
        }
        if (family_contains_device(path, major(source_status.st_rdev), minor(source_status.st_rdev), known)) active = true;
    }
    if (ferror(file)) *known = false;
    fclose(file);
    return active;
}

static bool mount_state(const char *path, bool *system_disk, bool *in_use, bool *known) {
    bool mounted = direct_mount_state(path, system_disk, known);
    const char *name = base_name(path);
    DIR *directory = opendir("/sys/class/block");
    if (directory == NULL) {
        *known = false;
        return mounted;
    }
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0) *known = false;
            break;
        }
        if (same_block_family(name, entry->d_name) && holders_mounted(entry->d_name, system_disk, in_use, known, 0)) mounted = true;
    }
    closedir(directory);
    if (swap_state(path, known)) *in_use = true;
    return mounted;
}

int dfx_inspect_device(const char *path, dfx_device *device, char error[DFX_ERROR_MAX]) {
    memset(device, 0, sizeof(*device));
    set_environment(device);
    if (!valid_device_path(path)) {
        snprintf(error, DFX_ERROR_MAX, "Chemin de périphérique absent, trop long, contenant un caractère de contrôle ou un UTF-8 invalide.");
        return -1;
    }
    struct stat status;
    if (lstat(path, &status) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Périphérique introuvable : %s", strerror(errno));
        return -1;
    }
    if (S_ISLNK(status.st_mode)) {
        snprintf(error, DFX_ERROR_MAX, "Refus : les liens symboliques ne sont pas acceptés.");
        return -1;
    }
    snprintf(device->path, sizeof(device->path), "%s", path);
    snprintf(device->model, sizeof(device->model), "Non déterminé");
    snprintf(device->firmware, sizeof(device->firmware), "Non déterminé");
    snprintf(device->transport, sizeof(device->transport), "inconnu");
    if (S_ISREG(status.st_mode)) {
        device->regular_file = true;
        device->kind = DFX_MEDIA_FILE;
        device->size_bytes = (uint64_t)status.st_size;
        device->logical_block_size = 1;
        device->whole_device = true;
        device->safety_state_known = true;
        device->stable_identity_unique = true;
        snprintf(device->model, sizeof(device->model), "Fichier de test");
        snprintf(device->firmware, sizeof(device->firmware), "n/a");
        snprintf(device->transport, sizeof(device->transport), "virtuel");
        snprintf(device->topology, sizeof(device->topology), "fichier");
        char content_hash[65];
        if (hash_regular_file(path, &status, content_hash, error) != 0) return -1;
        snprintf(device->stable_id, sizeof(device->stable_id), "fichier-%llu-%llu-%.64s", (unsigned long long)status.st_dev, (unsigned long long)status.st_ino, content_hash);
        device->object_identity_a = (uint64_t)status.st_dev;
        device->object_identity_b = (uint64_t)status.st_ino;
        return 0;
    }
    int descriptor = open(path, O_RDONLY | O_NONBLOCK);
    if (descriptor < 0) {
        snprintf(error, DFX_ERROR_MAX, "Ouverture impossible : %s", strerror(errno));
        return -1;
    }
    if (ioctl(descriptor, BLKGETSIZE64, &device->size_bytes) != 0 || ioctl(descriptor, BLKSSZGET, &device->logical_block_size) != 0) {
        close(descriptor);
        snprintf(error, DFX_ERROR_MAX, "Géométrie du disque inaccessible : %s", strerror(errno));
        return -1;
    }
    int read_only = 0;
    bool safety_state_known = true;
    if (ioctl(descriptor, BLKROGET, &read_only) != 0) safety_state_known = false;
    device->read_only = read_only != 0;
    close(descriptor);
    const char *name = base_name(path);
    device->object_identity_a = (uint64_t)status.st_rdev;
    char sys_path[DFX_PATH_MAX];
    char text[DFX_MODEL_MAX] = {0};
    snprintf(sys_path, sizeof(sys_path), "/sys/class/block/%s", name);
    device->whole_device = access(sys_path, F_OK) == 0;
    if (!device->whole_device) safety_state_known = false;
    snprintf(sys_path, sizeof(sys_path), "/sys/class/block/%s/partition", name);
    if (access(sys_path, F_OK) == 0) device->whole_device = false;
    if (read_device_text(name, "model", text, sizeof(text)) == 0) snprintf(device->model, sizeof(device->model), "%s", text);
    else snprintf(device->model, sizeof(device->model), "Non déterminé");
    const char *firmware_field = strncmp(name, "nvme", 4) == 0 ? "firmware_rev" : "rev";
    if (read_device_text(name, firmware_field, text, sizeof(text)) == 0) snprintf(device->firmware, sizeof(device->firmware), "%.120s", text);
    else snprintf(device->firmware, sizeof(device->firmware), "Non déterminé");
    snprintf(sys_path, sizeof(sys_path), "/sys/class/block/%s/removable", name);
    if (read_text(sys_path, text, sizeof(text)) == 0) device->removable = strcmp(text, "1") == 0;
    char device_path[DFX_PATH_MAX];
    snprintf(sys_path, sizeof(sys_path), "/sys/class/block/%s/device", name);
    device->internal = true;
    ssize_t device_path_length = readlink(sys_path, device_path, sizeof(device_path) - 1);
    if (device_path_length > 0) {
        device_path[device_path_length] = '\0';
        if (display_text_safe(device_path)) {
            snprintf(device->topology, sizeof(device->topology), "%.240s", device_path);
            if (strstr(device_path, "/usb") != NULL) {
                device->internal = false;
                snprintf(device->transport, sizeof(device->transport), "USB");
            } else if (strstr(device_path, "/nvme") != NULL) snprintf(device->transport, sizeof(device->transport), "NVMe");
            else if (strstr(device_path, "/ata") != NULL) snprintf(device->transport, sizeof(device->transport), "SATA");
            else snprintf(device->transport, sizeof(device->transport), "SCSI ou inconnu");
        } else safety_state_known = false;
    }
    snprintf(sys_path, sizeof(sys_path), "/sys/class/block/%s/queue/rotational", name);
    if (strncmp(name, "nvme", 4) == 0) device->kind = DFX_MEDIA_NVME;
    else if (read_text(sys_path, text, sizeof(text)) == 0) device->kind = strcmp(text, "1") == 0 ? DFX_MEDIA_HDD : DFX_MEDIA_SSD;
    else device->kind = DFX_MEDIA_UNKNOWN;
    if (read_device_text(name, "serial", text, sizeof(text)) == 0 && dfx_hardware_id_is_credible(text)) {
        snprintf(device->stable_id, sizeof(device->stable_id), "linux-%.200s-%llu", text, (unsigned long long)device->size_bytes);
        device->stable_identity_unique = true;
    }
    else snprintf(device->stable_id, sizeof(device->stable_id), "linux-%u-%u-%llu", major(status.st_rdev), minor(status.st_rdev), (unsigned long long)device->size_bytes);
    device->mounted = mount_state(path, &device->system_disk, &device->in_use, &safety_state_known);
    device->safety_state_known = safety_state_known;
    return 0;
}

int dfx_list_devices(FILE *out, char error[DFX_ERROR_MAX]) {
    DIR *directory = opendir("/sys/class/block");
    if (directory == NULL) {
        snprintf(error, DFX_ERROR_MAX, "Inventaire impossible : %s", strerror(errno));
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.' || strncmp(entry->d_name, "loop", 4) == 0 || strncmp(entry->d_name, "ram", 3) == 0) continue;
        char partition_path[DFX_PATH_MAX];
        snprintf(partition_path, sizeof(partition_path), "/sys/class/block/%s/partition", entry->d_name);
        if (access(partition_path, F_OK) == 0) continue;
        char path[DFX_PATH_MAX];
        snprintf(path, sizeof(path), "/dev/%s", entry->d_name);
        dfx_device device;
        if (dfx_inspect_device(path, &device, error) == 0) {
            char size[64];
            dfx_format_size(device.size_bytes, size, sizeof(size));
            fprintf(out, "%s\t%s\t%s\t%s\n", device.path, size, dfx_media_name(device.kind), device.mounted ? "monté" : "non monté");
        }
    }
    closedir(directory);
    error[0] = '\0';
    return 0;
}

#endif
