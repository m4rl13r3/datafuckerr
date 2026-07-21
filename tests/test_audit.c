#include "dfx.h"
#include "dfx_sha256.h"

#include <stdio.h>
#include <string.h>

static unsigned failures = 0;

static void expect(bool condition, const char *name) {
    if (condition) return;
    fprintf(stderr, "ÉCHEC : %s\n", name);
    failures++;
}

static dfx_job make_job(void) {
    dfx_job job;
    memset(&job, 0, sizeof(job));
    snprintf(job.device.path, sizeof(job.device.path), "cible-virtuelle-absente.img");
    snprintf(job.device.stable_id, sizeof(job.device.stable_id), "fichier-test-1");
    snprintf(job.device.model, sizeof(job.device.model), "Fichier de test");
    snprintf(job.device.firmware, sizeof(job.device.firmware), "n/a");
    snprintf(job.device.transport, sizeof(job.device.transport), "virtuel");
    snprintf(job.device.environment, sizeof(job.device.environment), "Système de test");
    snprintf(job.device.topology, sizeof(job.device.topology), "fichier");
    snprintf(job.device.qualification_id, sizeof(job.device.qualification_id), "VIRTUEL-TEST");
    snprintf(job.operator_id, sizeof(job.operator_id), "opérateur-test");
    snprintf(job.witness_id, sizeof(job.witness_id), "témoin-test");
    job.device.size_bytes = 8193U;
    job.device.logical_block_size = 1U;
    job.device.kind = DFX_MEDIA_FILE;
    job.device.regular_file = true;
    job.device.whole_device = true;
    job.device.stable_identity_unique = true;
    job.method = DFX_METHOD_CLEAR_ZERO;
    job.requested_method = DFX_METHOD_AUTO;
    job.full_verify = true;
    return job;
}

static void prepare_path(dfx_job *job, const char *suffix, char error[DFX_ERROR_MAX]) {
    expect(dfx_prepare_audit(job, error) == 0, "génération de l’identifiant d’opération");
    snprintf(job->audit_path, sizeof(job->audit_path), "audit-test-%.16s-%s.jsonl", job->operation_id, suffix);
    remove(job->audit_path);
}

static int verify(const char *path, char error[DFX_ERROR_MAX]) {
    FILE *output = tmpfile();
    if (output == NULL) return -1;
    int result = dfx_verify_audit_file(path, output, error);
    fclose(output);
    return result;
}

static int load_payload(const char *path, char *payload, size_t size) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return -1;
    char line[8192];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) return -1;
    const char *marker = strstr(line, ",\"précédente\":\"");
    if (marker == NULL) return -1;
    size_t length = (size_t)(marker - line);
    if (length + 2 > size) return -1;
    memcpy(payload, line, length);
    payload[length++] = '}';
    payload[length] = '\0';
    return 0;
}

static int replace_once(const char *input, const char *needle, const char *replacement, char *output, size_t size) {
    const char *position = strstr(input, needle);
    if (position == NULL) return -1;
    size_t prefix = (size_t)(position - input);
    size_t needle_length = strlen(needle);
    size_t replacement_length = strlen(replacement);
    size_t suffix = strlen(position + needle_length);
    if (prefix + replacement_length + suffix + 1 > size) return -1;
    memcpy(output, input, prefix);
    memcpy(output + prefix, replacement, replacement_length);
    memcpy(output + prefix + replacement_length, position + needle_length, suffix + 1);
    return 0;
}

static int write_rehashed_record(const char *path, const char *payload) {
    size_t payload_length = strlen(payload);
    if (payload_length == 0 || payload[payload_length - 1] != '}') return -1;
    char previous[65];
    memset(previous, '0', 64);
    previous[64] = '\0';
    char material[16384];
    int material_length = snprintf(material, sizeof(material), "%s%s", previous, payload);
    if (material_length < 0 || (size_t)material_length >= sizeof(material)) return -1;
    char hash[65];
    dfx_sha256_hex((const unsigned char *)material, (size_t)material_length, hash);
    FILE *file = fopen(path, "wb");
    if (file == NULL) return -1;
    int result = fprintf(file, "%.*s,\"précédente\":\"%s\",\"empreinte\":\"%s\"}\n", (int)payload_length - 1, payload, previous, hash) < 0 ? -1 : 0;
    if (fclose(file) != 0) result = -1;
    return result;
}

static void expect_canonical_rejection(const dfx_job *source, const char *suffix, const char *needle, const char *replacement, const char *name, char error[DFX_ERROR_MAX]) {
    char payload[8192];
    char modified[8192];
    char path[DFX_PATH_MAX + 64];
    snprintf(path, sizeof(path), "%s.%s", source->audit_path, suffix);
    remove(path);
    bool prepared = load_payload(source->audit_path, payload, sizeof(payload)) == 0
        && replace_once(payload, needle, replacement, modified, sizeof(modified)) == 0
        && write_rehashed_record(path, modified) == 0;
    expect(prepared, name);
    if (prepared) expect(verify(path, error) != 0 && strstr(error, "Schéma d’audit invalide") != NULL, name);
    remove(path);
}

int main(void) {
    char error[DFX_ERROR_MAX] = {0};
    dfx_job valid = make_job();
    prepare_path(&valid, "valide", error);
    expect(dfx_write_audit(&valid, "en_cours", "Début", error) == 0, "écriture du démarrage");
    snprintf(valid.final_stable_id, sizeof(valid.final_stable_id), "%s", valid.device.stable_id);
    valid.final_size_bytes = valid.device.size_bytes;
    valid.final_identity_observed = true;
    expect(dfx_write_audit(&valid, "réussi", "Fin", error) == 0, "écriture du succès");
    expect(verify(valid.audit_path, error) == 0, "opération complète valide");
    expect(dfx_write_audit(&valid, "réussi", "Second terminal", error) != 0 && strstr(error, "démarrage compatible") != NULL, "terminal dupliqué refusé avant écriture");
    dfx_job reused = valid;
    reused.final_stable_id[0] = '\0';
    reused.final_size_bytes = 0;
    reused.final_identity_observed = false;
    expect(dfx_write_audit(&reused, "en_cours", "Nouveau début", error) != 0 && strstr(error, "existe déjà") != NULL, "réutilisation d’identifiant pour un démarrage refusée");
    expect(verify(valid.audit_path, error) == 0, "journal préservé après terminal dupliqué");

    dfx_job refused = make_job();
    prepare_path(&refused, "refuse", error);
    expect(dfx_write_audit(&refused, "refusé", "Confirmation invalide", error) == 0, "refus autonome écrit");
    expect(dfx_write_audit(&refused, "refusé", "Second refus", error) != 0 && strstr(error, "existe déjà") != NULL, "réutilisation d’identifiant pour un refus rejetée");
    expect(verify(refused.audit_path, error) == 0, "refus autonome valide");
    expect_canonical_rejection(&refused, "duplique", ",\"détail\":", ",\"statut\":\"refusé\",\"détail\":", "clé dupliquée refusée malgré une chaîne recalculée", error);
    expect_canonical_rejection(&refused, "scalaire", "\"taille\":8193", "\"taille\":x", "scalaire non numérique refusé malgré une chaîne recalculée", error);
    expect_canonical_rejection(&refused, "inattendu", ",\"détail\":", ",\"champ_inattendu\":1,\"détail\":", "champ inattendu refusé malgré une chaîne recalculée", error);
    const char invalid_utf8[] = {(char)0xc3, (char)0x28, '\0'};
    expect_canonical_rejection(&refused, "utf8", "Confirmation invalide", invalid_utf8, "UTF-8 invalide refusé malgré une chaîne recalculée", error);

    dfx_job invalid_text = make_job();
    prepare_path(&invalid_text, "utf8-producteur", error);
    memcpy(invalid_text.operator_id, invalid_utf8, sizeof(invalid_utf8));
    expect(dfx_write_audit(&invalid_text, "en_cours", "Début", error) != 0 && strstr(error, "UTF-8 invalide") != NULL, "producteur refuse un texte UTF-8 invalide avant écriture");
    dfx_job invalid_control = make_job();
    prepare_path(&invalid_control, "controle-producteur", error);
    expect(dfx_write_audit(&invalid_control, "en_cours", "Ligne\nsuivante", error) != 0 && strstr(error, "contrôle") != NULL, "producteur refuse un contrôle avant écriture");

    dfx_job incomplete = make_job();
    prepare_path(&incomplete, "incomplet", error);
    expect(dfx_write_audit(&incomplete, "en_cours", "Début", error) == 0, "opération incomplète écrite");
    expect(verify(incomplete.audit_path, error) != 0 && strstr(error, "état terminal") != NULL, "opération incomplète refusée");

    dfx_job changed = make_job();
    prepare_path(&changed, "contexte", error);
    expect(dfx_write_audit(&changed, "en_cours", "Début", error) == 0, "contexte initial écrit");
    snprintf(changed.operator_id, sizeof(changed.operator_id), "autre-opérateur");
    expect(dfx_write_audit(&changed, "échoué", "Fin", error) != 0 && strstr(error, "Contexte modifié") != NULL, "contexte modifié refusé avant écriture");
    snprintf(changed.operator_id, sizeof(changed.operator_id), "opérateur-test");
    expect(dfx_write_audit(&changed, "échoué", "Fin", error) == 0, "contexte restauré accepté");
    expect(verify(changed.audit_path, error) == 0, "journal préservé après dérive de contexte");

    dfx_job interrupted = make_job();
    prepare_path(&interrupted, "interrompu", error);
    expect(dfx_write_audit(&interrupted, "en_cours", "Début", error) == 0, "démarrage interrompu écrit");
    dfx_job next = make_job();
    expect(dfx_prepare_audit(&next, error) == 0, "identifiant suivant généré");
    snprintf(next.audit_path, sizeof(next.audit_path), "%s", interrupted.audit_path);
    expect(dfx_write_audit(&next, "en_cours", "Nouveau début", error) != 0 && strstr(error, "état terminal") != NULL, "nouvelle opération refusée après journal incomplet");

    dfx_job replaced = make_job();
    prepare_path(&replaced, "remplace", error);
    expect(dfx_write_audit(&replaced, "en_cours", "Début", error) == 0, "journal initial écrit avant remplacement");
    char original_path[DFX_PATH_MAX + 16];
    snprintf(original_path, sizeof(original_path), "%s.ancien", replaced.audit_path);
    remove(original_path);
    expect(rename(replaced.audit_path, original_path) == 0, "journal initial déplacé");
    FILE *replacement = fopen(replaced.audit_path, "wb");
    expect(replacement != NULL, "journal de remplacement créé");
    if (replacement != NULL) fclose(replacement);
    expect(dfx_write_audit(&replaced, "échoué", "Fin", error) != 0 && strstr(error, "a changé") != NULL, "remplacement du journal refusé");

    dfx_job another = make_job();
    expect(dfx_prepare_audit(&another, error) == 0, "second identifiant généré");
    expect(strcmp(valid.operation_id, another.operation_id) != 0, "identifiants aléatoires distincts");

    dfx_job native = make_job();
    native.device.kind = DFX_MEDIA_NVME;
    native.device.regular_file = false;
    native.device.size_bytes = 8192U;
    native.device.logical_block_size = 512U;
    native.method = DFX_METHOD_PURGE_NATIVE;
    native.requested_method = DFX_METHOD_PURGE_NATIVE;
    native.native_action = DFX_NATIVE_NVME_CRYPTO;
    prepare_path(&native, "natif", error);
    expect(dfx_write_audit(&native, "en_cours", "Début natif", error) == 0, "démarrage natif sans ancien statut");
    snprintf(native.final_stable_id, sizeof(native.final_stable_id), "%s", native.device.stable_id);
    native.final_size_bytes = native.device.size_bytes;
    native.final_identity_observed = true;
    expect(dfx_write_audit(&native, "réussi", "Fin native sans statut", error) != 0 && strstr(error, "terminal incohérent") != NULL, "succès natif sans statut brut refusé avant écriture");
    native.native_status_observed = true;
    native.native_status_source = DFX_NATIVE_STATUS_SANITIZE_LOG;
    native.native_status_raw = 3U;
    expect(dfx_write_audit(&native, "réussi", "Fin native avec échec", error) != 0 && strstr(error, "terminal incohérent") != NULL, "succès natif avec statut d’échec refusé avant écriture");
    native.native_status_raw = 2U;
    expect(dfx_write_audit(&native, "réussi", "Fin native encore en cours", error) != 0 && strstr(error, "terminal incohérent") != NULL, "succès natif avec statut en cours refusé avant écriture");
    native.native_status_raw = 4U;
    expect(dfx_write_audit(&native, "réussi", "Fin native", error) == 0, "succès natif avec statut brut accepté");
    expect(verify(native.audit_path, error) == 0, "preuve native brute vérifiable");

    remove(valid.audit_path);
    remove(refused.audit_path);
    remove(incomplete.audit_path);
    remove(changed.audit_path);
    remove(invalid_text.audit_path);
    remove(invalid_control.audit_path);
    remove(interrupted.audit_path);
    remove(replaced.audit_path);
    remove(original_path);
    remove(native.audit_path);
    if (failures != 0) {
        fprintf(stderr, "%u test(s) d’audit en échec.\n", failures);
        return 1;
    }
    puts("Tests d’audit réussis.");
    return 0;
}
