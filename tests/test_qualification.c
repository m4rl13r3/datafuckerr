#include "dfx.h"

#include <stdio.h>
#include <string.h>

static unsigned failures = 0;

static void expect(bool condition, const char *name) {
    if (condition) return;
    fprintf(stderr, "ÉCHEC : %s\n", name);
    failures++;
}

static dfx_job matching_job(void) {
    dfx_job job;
    memset(&job, 0, sizeof(job));
    snprintf(job.device.model, sizeof(job.device.model), "MODÈLE-QUALIFIÉ");
    snprintf(job.device.firmware, sizeof(job.device.firmware), "FW-1");
    snprintf(job.device.transport, sizeof(job.device.transport), "SATA");
    snprintf(job.device.environment, sizeof(job.device.environment), "SYSTÈME-TEST");
    snprintf(job.device.topology, sizeof(job.device.topology), "TOPOLOGIE-TEST");
    job.device.kind = DFX_MEDIA_HDD;
    job.device.size_bytes = 1000000U;
    job.device.logical_block_size = 512U;
    job.method = DFX_METHOD_CLEAR_ZERO;
    job.native_action = DFX_NATIVE_NONE;
    job.full_verify = true;
    return job;
}

int main(void) {
    dfx_job job = matching_job();
    dfx_apply_qualification(&job);
    expect(job.device.qualified, "tuple exact qualifié");
    expect(strcmp(job.device.qualification_id, "Q3-TEST-001") == 0, "identifiant de qualification exact");

    job = matching_job();
    snprintf(job.device.firmware, sizeof(job.device.firmware), "FW-2");
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "firmware différent refusé");

    job = matching_job();
    snprintf(job.device.environment, sizeof(job.device.environment), "AUTRE-SYSTÈME");
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "environnement différent refusé");

    job = matching_job();
    snprintf(job.device.topology, sizeof(job.device.topology), "AUTRE-TOPOLOGIE");
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "topologie différente refusée");

    job = matching_job();
    job.method = DFX_METHOD_PURGE_NATIVE;
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "méthode différente refusée");

    job = matching_job();
    job.device.size_bytes++;
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "capacité différente refusée");

    job = matching_job();
    job.device.logical_block_size = 4096U;
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "taille de bloc différente refusée");

    job = matching_job();
    job.full_verify = false;
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "vérification échantillonnée non qualifiée refusée");

    job = matching_job();
    job.native_action = DFX_NATIVE_NVME_CRYPTO;
    dfx_apply_qualification(&job);
    expect(!job.device.qualified, "sous-méthode native différente refusée");

    job = matching_job();
    job.device.regular_file = true;
    dfx_apply_qualification(&job);
    expect(job.device.qualified, "fichier virtuel qualifié pour les tests");
    expect(strcmp(job.device.qualification_id, "VIRTUEL-TEST") == 0, "qualification virtuelle explicite");

    if (failures != 0) {
        fprintf(stderr, "%u test(s) de qualification en échec.\n", failures);
        return 1;
    }
    puts("Tests de qualification réussis.");
    return 0;
}
