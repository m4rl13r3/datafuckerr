#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "dfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

static void usage(FILE *out) {
    fprintf(out,
        "diskpurge %s\n"
        "Usage :\n"
        "  diskpurge list\n"
        "  diskpurge verify-audit <fichier.jsonl>\n"
        "  diskpurge inspect <périphérique>\n"
        "  diskpurge plan <périphérique> [--method auto|clear|purge|destroy] [--verify full|sample]\n"
        "  diskpurge erase <périphérique> --confirm <identifiant> [--method auto|clear|purge] [--verify full|sample] [--audit <fichier>] [--operator <identifiant>] [--witness <identifiant>] [--acknowledge-data-loss] [--allow-internal] [--lab-mode]\n"
        "\nLe mode erase est irréversible. Sans erase, aucune donnée n’est modifiée.\n",
        DFX_BUILD_VERSION);
}

static void handle_signal(int value) {
    (void)value;
    dfx_request_cancel();
}

static bool install_signal_handlers(char error[DFX_ERROR_MAX]) {
#ifdef _WIN32
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        snprintf(error, DFX_ERROR_MAX, "Installation du gestionnaire SIGINT impossible.");
        return false;
    }
#ifdef SIGTERM
    if (signal(SIGTERM, handle_signal) == SIG_ERR) {
        snprintf(error, DFX_ERROR_MAX, "Installation du gestionnaire SIGTERM impossible.");
        return false;
    }
#endif
#else
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    if (sigemptyset(&action.sa_mask) != 0 || sigaction(SIGINT, &action, NULL) != 0 || sigaction(SIGTERM, &action, NULL) != 0) {
        snprintf(error, DFX_ERROR_MAX, "Installation des gestionnaires d’interruption impossible.");
        return false;
    }
#endif
    return true;
}

static void print_device(const dfx_device *device) {
    char size[64];
    dfx_format_size(device->size_bytes, size, sizeof(size));
    printf("Périphérique : %s\n", device->path);
    printf("Identifiant  : %s\n", device->stable_id);
    printf("Modèle       : %s\n", device->model);
    printf("Firmware     : %s\n", device->firmware);
    printf("Transport    : %s\n", device->transport);
    printf("Environnement : %s\n", device->environment);
    printf("Topologie      : %s\n", device->topology);
    printf("Taille       : %s\n", size);
    printf("Support      : %s\n", dfx_media_name(device->kind));
    printf("Amovible     : %s\n", device->removable ? "oui" : "non");
    printf("Interne      : %s\n", device->internal ? "oui" : "non");
    printf("Monté        : %s\n", device->mounted ? "oui" : "non");
    printf("Utilisé      : %s\n", device->in_use ? "oui" : "non");
    printf("État de sûreté : %s\n", device->safety_state_known ? "établi" : "inconnu");
    printf("Identité unique : %s\n", device->stable_identity_unique ? "oui" : "non");
    printf("Disque système : %s\n", device->system_disk ? "oui" : "non");
    printf("Lecture seule  : %s\n", device->read_only ? "oui" : "non");
    printf("Qualification  : %s\n", device->qualified ? device->qualification_id : "aucune");
}

static bool parse_method(const char *value, dfx_method *method) {
    if (strcmp(value, "auto") == 0) *method = DFX_METHOD_AUTO;
    else if (strcmp(value, "clear") == 0) *method = DFX_METHOD_CLEAR_ZERO;
    else if (strcmp(value, "purge") == 0) *method = DFX_METHOD_PURGE_NATIVE;
    else if (strcmp(value, "destroy") == 0) *method = DFX_METHOD_DESTROY_PHYSICAL;
    else return false;
    return true;
}

static bool copy_option(char *destination, size_t size, const char *value, const char *name) {
    if (strlen(value) >= size) {
        fprintf(stderr, "Valeur trop longue pour %s.\n", name);
        return false;
    }
    if (!dfx_text_is_safe(value)) {
        fprintf(stderr, "Texte de contrôle ou UTF-8 invalide dans %s.\n", name);
        return false;
    }
    memcpy(destination, value, strlen(value) + 1);
    return true;
}

static bool accept_once(unsigned *seen, unsigned flag, const char *name) {
    if ((*seen & flag) != 0U) {
        fprintf(stderr, "Option dupliquée : %s\n", name);
        return false;
    }
    *seen |= flag;
    return true;
}

static void progress(uint64_t done, uint64_t total, void *context) {
    (void)context;
    double percent = total == 0 ? 100.0 : (double)done * 100.0 / (double)total;
    fprintf(stderr, "\rEffacement : %6.2f %%", percent);
    fflush(stderr);
}

static int observe_final_identity(dfx_job *job, char error[DFX_ERROR_MAX]) {
    dfx_device final_device;
    if (dfx_inspect_device(job->device.path, &final_device, error) != 0) return -1;
    if (final_device.object_identity_a != job->device.object_identity_a || final_device.object_identity_b != job->device.object_identity_b || final_device.size_bytes != job->device.size_bytes || final_device.logical_block_size != job->device.logical_block_size || final_device.kind != job->device.kind || final_device.stable_identity_unique != job->device.stable_identity_unique || (!job->device.regular_file && strcmp(final_device.stable_id, job->device.stable_id) != 0) || strcmp(final_device.model, job->device.model) != 0 || strcmp(final_device.firmware, job->device.firmware) != 0 || strcmp(final_device.transport, job->device.transport) != 0 || strcmp(final_device.environment, job->device.environment) != 0 || strcmp(final_device.topology, job->device.topology) != 0) {
        snprintf(error, DFX_ERROR_MAX, "L’identité ou la géométrie observée après l’opération ne correspond plus à la cible confirmée.");
        return -1;
    }
    dfx_job final_job = *job;
    final_job.device = final_device;
    dfx_apply_qualification(&final_job);
    if (!job->device.regular_file && !job->lab_mode && (!final_job.device.qualified || strcmp(final_job.device.qualification_id, job->device.qualification_id) != 0)) {
        snprintf(error, DFX_ERROR_MAX, "La qualification observée après l’opération ne correspond plus au plan confirmé.");
        return -1;
    }
    snprintf(job->final_stable_id, sizeof(job->final_stable_id), "%s", final_device.stable_id);
    job->final_size_bytes = final_device.size_bytes;
    job->final_identity_observed = true;
    return 0;
}

int main(int argc, char **argv) {
    char error[DFX_ERROR_MAX] = {0};
    if (argc < 2) {
        usage(stderr);
        return 2;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0) {
        if (argc != 2) {
            usage(stderr);
            return 2;
        }
        usage(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        if (argc != 2) {
            usage(stderr);
            return 2;
        }
        puts(DFX_BUILD_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) {
            usage(stderr);
            return 2;
        }
        if (dfx_list_devices(stdout, error) != 0) {
            fprintf(stderr, "Erreur : %s\n", error);
            return 1;
        }
        return 0;
    }
    if (strcmp(argv[1], "verify-audit") == 0) {
        if (argc != 3) {
            usage(stderr);
            return 2;
        }
        if (dfx_verify_audit_file(argv[2], stdout, error) != 0) {
            fprintf(stderr, "Audit invalide : %s\n", error);
            return 1;
        }
        return 0;
    }
    if (argc < 3) {
        usage(stderr);
        return 2;
    }
    dfx_job job;
    memset(&job, 0, sizeof(job));
    job.method = DFX_METHOD_AUTO;
    job.requested_method = DFX_METHOD_AUTO;
    job.full_verify = true;
    if (dfx_inspect_device(argv[2], &job.device, error) != 0) {
        fprintf(stderr, "Erreur : %s\n", error);
        return 1;
    }
    if (strcmp(argv[1], "inspect") == 0) {
        if (argc != 3) {
            usage(stderr);
            return 2;
        }
        print_device(&job.device);
        return 0;
    }
    bool planning = strcmp(argv[1], "plan") == 0;
    bool erasing = strcmp(argv[1], "erase") == 0;
    if (!planning && !erasing) {
        usage(stderr);
        return 2;
    }
    unsigned seen = 0;
    for (int index = 3; index < argc; index++) {
        if (strcmp(argv[index], "--method") == 0 && index + 1 < argc) {
            if (!accept_once(&seen, 1U << 0, "--method")) return 2;
            if (!parse_method(argv[++index], &job.method)) {
                fprintf(stderr, "Méthode invalide : %s\n", argv[index]);
                return 2;
            }
        }
        else if (erasing && strcmp(argv[index], "--confirm") == 0 && index + 1 < argc) {
            if (!accept_once(&seen, 1U << 1, "--confirm")) return 2;
            if (!copy_option(job.confirmation, sizeof(job.confirmation), argv[++index], "--confirm")) return 2;
        }
        else if (strcmp(argv[index], "--verify") == 0 && index + 1 < argc) {
            if (!accept_once(&seen, 1U << 2, "--verify")) return 2;
            job.verification_explicit = true;
            const char *value = argv[++index];
            if (strcmp(value, "full") == 0) job.full_verify = true;
            else if (strcmp(value, "sample") == 0) job.full_verify = false;
            else {
                fprintf(stderr, "Mode de vérification invalide : %s\n", value);
                return 2;
            }
        }
        else if (erasing && strcmp(argv[index], "--audit") == 0 && index + 1 < argc) {
            if (!accept_once(&seen, 1U << 3, "--audit")) return 2;
            if (!copy_option(job.audit_path, sizeof(job.audit_path), argv[++index], "--audit")) return 2;
        }
        else if (erasing && strcmp(argv[index], "--operator") == 0 && index + 1 < argc) {
            if (!accept_once(&seen, 1U << 4, "--operator")) return 2;
            if (!copy_option(job.operator_id, sizeof(job.operator_id), argv[++index], "--operator")) return 2;
        }
        else if (erasing && strcmp(argv[index], "--witness") == 0 && index + 1 < argc) {
            if (!accept_once(&seen, 1U << 5, "--witness")) return 2;
            if (!copy_option(job.witness_id, sizeof(job.witness_id), argv[++index], "--witness")) return 2;
        }
        else if (erasing && strcmp(argv[index], "--acknowledge-data-loss") == 0) {
            if (!accept_once(&seen, 1U << 6, "--acknowledge-data-loss")) return 2;
            job.acknowledged = true;
        }
        else if (erasing && strcmp(argv[index], "--allow-internal") == 0) {
            if (!accept_once(&seen, 1U << 7, "--allow-internal")) return 2;
            job.allow_internal = true;
        }
        else if (erasing && strcmp(argv[index], "--lab-mode") == 0) {
            if (!accept_once(&seen, 1U << 8, "--lab-mode")) return 2;
            if (!dfx_lab_mode_available()) {
                fprintf(stderr, "Refus : ce binaire n’a pas été compilé avec le mode laboratoire.\n");
                return 2;
            }
            job.lab_mode = true;
        }
        else {
            fprintf(stderr, "Option invalide : %s\n", argv[index]);
            return 2;
        }
    }
    job.requested_method = job.method;
    if (dfx_resolve_method(&job, error) != 0) {
        if (planning) {
            print_device(&job.device);
            printf("Méthode      : indisponible\n");
            printf("Exécutable   : non — %s\n", error);
            return 3;
        }
        fprintf(stderr, "Refus : %s\n", error);
        return 1;
    }
    if (planning) {
        print_device(&job.device);
        printf("Méthode      : %s\n", dfx_method_name(job.method));
        if (job.method == DFX_METHOD_PURGE_NATIVE) printf("Sous-méthode : %s\n", dfx_native_action_name(job.native_action));
        if (job.method == DFX_METHOD_CLEAR_ZERO) printf("Vérification : %s\n", job.full_verify ? "complète" : "échantillonnée");
        if (dfx_validate_job(&job, error) != 0) {
            printf("Exécutable   : non — %s\n", error);
            return 3;
        }
        printf("Exécutable   : oui\n");
        return 0;
    }
    job.execute = true;
    if (job.audit_path[0] != '\0' && dfx_prepare_audit(&job, error) != 0) {
        fprintf(stderr, "Erreur : %s\n", error);
        return 1;
    }
    if (!install_signal_handlers(error)) {
        fprintf(stderr, "Erreur : %s\n", error);
        return 1;
    }
    if (dfx_validate_job(&job, error) != 0) {
        fprintf(stderr, "Refus : %s\n", error);
        char refusal[DFX_ERROR_MAX];
        snprintf(refusal, sizeof(refusal), "%s", error);
        if (dfx_write_audit(&job, "refusé", refusal, error) != 0) fprintf(stderr, "Erreur d’audit : %s\n", error);
        return 1;
    }
    if (dfx_write_audit(&job, "en_cours", "Effacement lancé", error) != 0) {
        fprintf(stderr, "Erreur : %s\n", error);
        return 1;
    }
    int result = dfx_run_job(&job, progress, NULL, error);
    if (result == 0 && observe_final_identity(&job, error) != 0) result = DFX_RESULT_INDETERMINATE;
    if (result != 0) {
        const char *status = result == DFX_RESULT_INDETERMINATE ? "indéterminé" : "échoué";
        fprintf(stderr, "\n%s : %s\n", result == DFX_RESULT_INDETERMINATE ? "État indéterminé" : "Échec", error);
        char failure[DFX_ERROR_MAX];
        snprintf(failure, sizeof(failure), "%s", error);
        char observation_error[DFX_ERROR_MAX] = {0};
        observe_final_identity(&job, observation_error);
        if (dfx_write_audit(&job, status, failure, error) != 0) {
            fprintf(stderr, "État indéterminé : le résultat n’a pas pu être persisté dans le journal d’audit : %s\n", error);
            return 4;
        }
        return result == DFX_RESULT_INDETERMINATE ? 4 : 1;
    }
    fprintf(stderr, "\n");
    const char *success_detail = job.method == DFX_METHOD_PURGE_NATIVE ? "Purge native terminée et confirmée par le contrôleur" : "Écriture et vérification terminées";
    if (dfx_write_audit(&job, "réussi", success_detail, error) != 0) {
        fprintf(stderr, "État indéterminé : l’effacement s’est terminé, mais son état terminal d’audit n’a pas pu être persisté : %s\n", error);
        return 4;
    }
    puts("Effacement et vérification terminés.");
    return 0;
}
