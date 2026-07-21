#include "dfx.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static unsigned failures = 0;

static void expect(bool condition, const char *name) {
    if (condition) return;
    fprintf(stderr, "ÉCHEC : %s\n", name);
    failures++;
}

static dfx_job physical_job(void) {
    dfx_job job;
    memset(&job, 0, sizeof(job));
    snprintf(job.device.path, sizeof(job.device.path), "/dev/disque-test");
    snprintf(job.device.stable_id, sizeof(job.device.stable_id), "SERIE-TEST");
    snprintf(job.confirmation, sizeof(job.confirmation), "SERIE-TEST");
    snprintf(job.audit_path, sizeof(job.audit_path), "audit-test.jsonl");
    snprintf(job.operator_id, sizeof(job.operator_id), "opérateur-test");
    snprintf(job.witness_id, sizeof(job.witness_id), "témoin-test");
    job.device.kind = DFX_MEDIA_NVME;
    job.device.size_bytes = 1048576U;
    job.device.logical_block_size = 512U;
    job.device.whole_device = true;
    job.device.safety_state_known = true;
    job.device.stable_identity_unique = true;
    job.device.qualified = true;
    job.method = DFX_METHOD_PURGE_NATIVE;
    job.native_action = DFX_NATIVE_NVME_CRYPTO;
    job.full_verify = true;
    job.execute = true;
    job.acknowledged = true;
    return job;
}

static void expect_result(dfx_job job, bool accepted, const char *diagnostic, const char *name) {
    char error[DFX_ERROR_MAX] = {0};
    int result = dfx_validate_job(&job, error);
    bool matches = accepted ? result == 0 : result != 0 && strstr(error, diagnostic) != NULL;
    if (matches) return;
    fprintf(stderr, "ÉCHEC : %s — résultat=%d, diagnostic=%s\n", name, result, error);
    failures++;
}

int main(void) {
    const char invalid_utf8[] = {(char)0xc3, (char)0x28, '\0'};
    expect(dfx_text_is_safe("Opérateur été"), "UTF-8 valide accepté");
    expect(!dfx_text_is_safe("ligne\nseconde"), "caractère de contrôle refusé");
    expect(!dfx_text_is_safe(invalid_utf8), "UTF-8 invalide refusé");
    expect(dfx_hardware_id_is_credible("SERIE-A19Z"), "identifiant matériel crédible accepté");
    expect(!dfx_hardware_id_is_credible("00000000"), "identifiant matériel générique refusé");
    expect(!dfx_hardware_id_is_credible("UNKNOWN"), "identifiant matériel inconnu refusé");
    expect(!dfx_hardware_id_is_credible("AAAAAA"), "identifiant matériel uniforme refusé");
    expect(!dfx_hardware_id_is_credible(invalid_utf8), "identifiant matériel UTF-8 invalide refusé");

    dfx_job job = physical_job();
    expect_result(job, true, "", "tâche physique complète acceptée");

    job = physical_job();
    job.device.qualified = false;
    expect_result(job, false, "n’est pas qualifié", "matériel non qualifié refusé");

    job = physical_job();
    job.device.qualified = false;
    job.lab_mode = true;
    if (dfx_lab_mode_available()) expect_result(job, true, "", "mode laboratoire explicite accepté par un binaire laboratoire");
    else expect_result(job, false, "mode laboratoire n’est pas disponible", "mode laboratoire forgé refusé dans le binaire standard");

    job = physical_job();
    job.device.system_disk = true;
    expect_result(job, false, "système", "disque système refusé");

    job = physical_job();
    job.device.mounted = true;
    expect_result(job, false, "montée", "disque monté refusé");

    job = physical_job();
    job.device.in_use = true;
    expect_result(job, false, "détenteur actif", "détenteur actif refusé");

    job = physical_job();
    job.device.safety_state_known = false;
    expect_result(job, false, "sans ambiguïté", "état de sûreté inconnu refusé");

    job = physical_job();
    job.device.read_only = true;
    expect_result(job, false, "lecture seule", "lecture seule refusée");

    job = physical_job();
    job.device.whole_device = false;
    expect_result(job, false, "partition", "partition refusée");

    job = physical_job();
    job.device.stable_identity_unique = false;
    expect_result(job, false, "identifiant matériel unique", "identité matérielle ambiguë refusée");

    job = physical_job();
    job.device.size_bytes = 0;
    job.device.logical_block_size = 512U;
    expect_result(job, false, "capacité", "capacité physique nulle refusée");

    job = physical_job();
    job.device.logical_block_size = 0;
    expect_result(job, false, "taille de bloc", "taille de bloc physique nulle refusée");

    job = physical_job();
    job.device.size_bytes++;
    expect_result(job, false, "incohérente", "capacité physique non alignée refusée");

    job = physical_job();
    job.device.kind = DFX_MEDIA_UNKNOWN;
    expect_result(job, false, "type de support", "support inconnu refusé");

    job = physical_job();
    job.device.kind = DFX_MEDIA_HDD;
    job.method = DFX_METHOD_CLEAR_ZERO;
    job.native_action = DFX_NATIVE_NONE;
    expect_result(job, false, "descripteur destructif", "écrasement physique standard désactivé sans identité liée au descripteur");

    job = physical_job();
    job.device.kind = DFX_MEDIA_SSD;
    job.method = DFX_METHOD_CLEAR_ZERO;
    job.native_action = DFX_NATIVE_NONE;
    expect_result(job, false, "blocs remappés", "écrasement SSD refusé");

    job = physical_job();
    snprintf(job.confirmation, sizeof(job.confirmation), "AUTRE");
    expect_result(job, false, "Confirmation invalide", "mauvaise identité refusée");

    job = physical_job();
    job.acknowledged = false;
    expect_result(job, false, "acknowledge-data-loss", "acquittement absent refusé");

    job = physical_job();
    job.audit_path[0] = '\0';
    expect_result(job, false, "audit", "audit absent refusé");

    job = physical_job();
    job.operator_id[0] = '\0';
    expect_result(job, false, "operator", "opérateur absent refusé");

    job = physical_job();
    job.witness_id[0] = '\0';
    expect_result(job, false, "witness", "témoin absent refusé");

    job = physical_job();
    snprintf(job.witness_id, sizeof(job.witness_id), "%s", job.operator_id);
    expect_result(job, false, "distinctes", "identités identiques refusées");

    job = physical_job();
    job.device.internal = true;
    expect_result(job, false, "allow-internal", "disque interne refusé par défaut");
    job.allow_internal = true;
    expect_result(job, true, "", "disque interne explicitement autorisé");

    job = physical_job();
    job.method = DFX_METHOD_DESTROY_PHYSICAL;
    expect_result(job, false, "station certifiée", "fausse destruction logicielle refusée");

    job = physical_job();
    job.method = DFX_METHOD_PURGE_NATIVE;
    job.verification_explicit = true;
    expect_result(job, false, "preuve d’une purge native", "vérification logique ambiguë refusée pour une purge native");

    if (failures != 0) {
        fprintf(stderr, "%u test(s) de sûreté en échec.\n", failures);
        return 1;
    }
    puts("Tests de sûreté réussis.");
    return 0;
}
