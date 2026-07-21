# Publication d’une version

Le workflow de publication construit les trois variantes depuis le même commit, avec le mode laboratoire explicitement désactivé. Il exécute les tests, crée une archive et une application autonome par plateforme, produit les nomenclatures et sommes SHA-256, puis génère une attestation de provenance GitHub pour chaque fichier. Un tag `vX.Y.Z` correspondant exactement au fichier `VERSION` crée ou complète une GitHub Release.

Ce mécanisme rend une publication traçable. Il ne qualifie aucun matériel et ne transforme pas une version alpha en produit de production. Tant que la matrice Q3 est vide, les variantes publiées doivent refuser les périphériques physiques et les versions `v0.*` restent des prépublications.

## Préconditions du dépôt

- protéger la branche de publication et imposer la CI ainsi que deux revues sur les chemins destructifs ;
- protéger l’environnement GitHub `publication` avec des approbateurs indépendants ;
- limiter la création des tags de publication aux mainteneurs désignés ;
- imposer des tags annotés et signés, puis vérifier la signature avant validation de l’environnement ;
- conserver `DISKPURGE_ENABLE_LAB_MODE=OFF` dans le workflow public ;
- rendre les règles, les responsables et les preuves de chaque case P0 auditables.

## Préparer une version

1. Mettre à jour `VERSION` et `CHANGELOG.md` dans la candidate.
2. Exécuter la CI complète et les analyses prévues par la checklist.
3. Vérifier qu’aucun tuple non Q3 n’est activé dans la table de qualification compilée.
4. Geler le commit, faire approuver les preuves et créer un tag annoté signé correspondant exactement à `v` suivi de `VERSION`.
5. Pousser le tag. L’environnement `publication` doit suspendre l’envoi des fichiers jusqu’à l’approbation humaine.
6. Vérifier les dix-sept fichiers joints à la prépublication, leurs attestations et les notes générées avant toute promotion.

Exemple de création locale d’un tag, à adapter à l’identité de signature de l’organisation :

~~~sh
version="$(cat VERSION)"
git tag -s "v$version" -m "datafuckerr v$version"
git push origin "v$version"
~~~

## Contenu des lots

Chaque plateforme produit cinq fichiers, auxquels s’ajoutent l’image amorçable amd64 et sa somme SHA-256 :

- une archive `.tar.gz` ou `.zip` contenant le binaire, l’interface Qt, le générateur de rapport, la licence, les documents de gouvernance, la documentation et un manifeste de fichiers ;
- une nomenclature `.cdx.json` CycloneDX 1.5 reliant l’application, le binaire, l’archive, la plateforme et le commit ;
- un fichier `.sha256` couvrant exactement l’archive et la nomenclature.
- une application autonome : AppImage Linux x64, DMG macOS Apple Silicon ou installateur Windows x64 ;
- un fichier `.sha256` couvrant exactement l’application autonome.

Le manifeste et la nomenclature déclarent que le mode laboratoire est désactivé. Le script de vérification refuse les chemins dangereux, liens symboliques, sommes incohérentes, versions divergentes et lots incomplets. La nomenclature est volontairement minimale parce que le binaire n’embarque aucune dépendance applicative tierce ; elle ne décrit pas tous les composants du système d’exploitation ni du compilateur.

L’empaquetage des archives est déterministe à binaire, commit et plateforme identiques. Les applications autonomes incorporent la pile Python et Qt fournie au moment de la construction. Les images GitHub `ubuntu-22.04`, `macos-15` et `windows-2025` peuvent recevoir des mises à jour : les exécutables ne sont donc pas encore revendiqués comme reproductibles bit à bit. La provenance doit être conservée pour identifier l’image réellement utilisée.

Les paquets macOS et Windows de l’alpha ne sont ni signés avec un certificat développeur ni notarisés. Une somme ou une attestation de provenance ne remplace pas la signature de code reconnue par le système. Leur diffusion doit conserver cette limite visible jusqu’à la mise en place de certificats protégés et d’une procédure de signature indépendante.

Une construction locale de contrôle peut être préparée ainsi après compilation :

~~~sh
python3 tools/release/package_release.py \
  --binary build/diskpurge \
  --platform local-$(uname -m) \
  --output-dir dist \
  --repository propriétaire/diskpurge \
  --source-commit "$(git rev-parse HEAD)"
python3 tools/release/verify_release.py dist
~~~

## Vérifier un téléchargement

La somme protège contre une erreur de transport seulement si le fichier de sommes provient d’un canal authentifié. L’attestation GitHub lie l’artefact au workflow et au commit par une identité OIDC ; elle doit être vérifiée en plus de la somme.

~~~sh
sha256sum --check diskpurge-0.2.4-linux-x64.sha256
sha256sum --check datafuckerr-0.2.4-linux-x64.AppImage.sha256
gh attestation verify diskpurge-0.2.4-linux-x64.tar.gz --repo propriétaire/diskpurge
gh attestation verify datafuckerr-0.2.4-linux-x64.AppImage --repo propriétaire/diskpurge
~~~

Sur macOS, remplacer `sha256sum` par `shasum -a 256` et comparer les deux lignes. Sous Windows, utiliser `Get-FileHash -Algorithm SHA256`.

## Révocation

Une publication ne doit jamais être supprimée silencieusement. En cas de défaut critique, retirer immédiatement les tuples concernés de la qualification, marquer la GitHub Release comme retirée, publier un avis de sécurité et conserver les artefacts ainsi que les preuves nécessaires à l’enquête. Une nouvelle version corrigée reçoit un nouveau tag ; un tag publié n’est jamais déplacé.
