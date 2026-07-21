#include "dfx.h"
#include "erase_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned failures = 0;

static void expect(bool condition, const char *name) {
    if (condition) return;
    fprintf(stderr, "ÉCHEC : %s\n", name);
    failures++;
}

static int fill_file(const char *path, unsigned char value, size_t size) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) return -1;
    for (size_t index = 0; index < size; index++) {
        if (fputc(value, file) == EOF) {
            fclose(file);
            return -1;
        }
    }
    return fclose(file);
}

static bool file_has_value(const char *path, unsigned char value, size_t size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    bool valid = true;
    for (size_t index = 0; index < size; index++) {
        if (fgetc(file) != value) {
            valid = false;
            break;
        }
    }
    if (fgetc(file) != EOF) valid = false;
    fclose(file);
    return valid;
}

static dfx_job inspect_job(const char *path, char error[DFX_ERROR_MAX]) {
    dfx_job job;
    memset(&job, 0, sizeof(job));
    job.method = DFX_METHOD_AUTO;
    job.requested_method = DFX_METHOD_AUTO;
    job.full_verify = true;
    if (dfx_inspect_device(path, &job.device, error) != 0 || dfx_resolve_method(&job, error) != 0) return job;
    job.execute = true;
    snprintf(job.confirmation, sizeof(job.confirmation), "%s", job.device.stable_id);
    return job;
}

int main(void) {
    const char *changed_path = "erase-test-changed.img";
    const char *cancelled_path = "erase-test-cancelled.img";
    char error[DFX_ERROR_MAX] = {0};
    remove(changed_path);
    remove(cancelled_path);

    expect(fill_file(changed_path, 0x31U, 4097U) == 0, "création de la cible remplacée");
    dfx_job changed = inspect_job(changed_path, error);
    expect(changed.device.stable_id[0] != '\0', "inspection initiale de la cible remplacée");
    expect(remove(changed_path) == 0, "suppression de la première cible");
    expect(fill_file(changed_path, 0x52U, 4097U) == 0, "création de la cible de remplacement");
    FILE *replacement = fopen(changed_path, "r+b");
    expect(replacement != NULL, "ouverture déterministe de la cible de remplacement");
    if (replacement != NULL) {
        expect(dfx_validate_open_target(replacement, &changed, error) != 0 && strstr(error, "confirmé") != NULL, "descripteur de remplacement refusé face à l’identité confirmée");
        fclose(replacement);
    }
    expect(dfx_run_job(&changed, NULL, NULL, error) != 0 && strstr(error, "identité") != NULL, "identité périmée refusée");
    expect(file_has_value(changed_path, 0x52U, 4097U), "cible de remplacement intacte");

    expect(fill_file(cancelled_path, 0x73U, 8193U) == 0, "création de la cible annulée");
    dfx_job cancelled = inspect_job(cancelled_path, error);
    dfx_request_cancel();
    expect(dfx_run_job(&cancelled, NULL, NULL, error) != 0 && strstr(error, "partiellement effacé") != NULL, "annulation avant la première écriture signalée");
    expect(file_has_value(cancelled_path, 0x73U, 8193U), "annulation avant écriture sans modification");

    remove(changed_path);
    remove(cancelled_path);
    if (failures != 0) {
        fprintf(stderr, "%u test(s) d’effacement en échec.\n", failures);
        return 1;
    }
    puts("Tests d’effacement réussis.");
    return 0;
}
