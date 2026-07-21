# datafuckerr

Suite open source d’effacement de supports de stockage. Son moteur C17 `diskpurge` fournit l’inventaire, la planification et l’exécution ; l’interface Qt 6 `datafuckerr` l’expose sur les plateformes prises en charge.

> **Statut : prépublication alpha 0.2.3.** Cette version est destinée aux fichiers virtuels, aux démonstrations et au développement. N’utilisez pas `diskpurge` sur un support réel ou sur des données client avant d’avoir satisfait les critères de qualification matérielle et de publication du projet.

## Portée actuelle

| Fonction | Linux | macOS | Windows |
| --- | --- | --- | --- |
| Inventaire et inspection | Expérimental | Expérimental | Expérimental |
| Écrasement à zéro d’un HDD ou d’un fichier | Expérimental | Expérimental | Non qualifié |
| Purge native NVMe directe | Expérimental | Indisponible | Indisponible |
| Purge native ATA ou SCSI | Indisponible | Indisponible | Indisponible |
| Destruction physique | Hors logiciel | Hors logiciel | Hors logiciel |

Sous Linux, le backend NVMe direct lit les capacités du contrôleur, privilégie le crypto-erase, utilise sinon le block-erase, puis suit le journal Sanitize. Cette implémentation n’est pas encore qualifiée sur une matrice de matériels sacrifiables.

Le binaire standard 0.2.3 ne contient aucun tuple matériel réel qualifié : il refuse donc actuellement tout `erase` visant un disque physique. Les fichiers réguliers de test sont les seules cibles qualifiées par la table distribuée. Un build de laboratoire est marqué par le suffixe de version `-lab` et reste réservé aux supports sacrifiables isolés ; il n’est pas un binaire de production.

Le type de connecteur — USB, USB-C, SATA ou PCIe — ne garantit pas la disponibilité d’une méthode. Un pont USB peut filtrer les commandes natives, modifier l’identité exposée ou masquer les capacités du support.

## Interface graphique, rapport et média amorçable

datafuckerr fournit une interface Qt 6 moderne qui pilote exclusivement le cœur C standard, sans interpréteur de commandes et sans transmettre le mode laboratoire. Elle propose une navigation clavier, un contraste renforcé, une échelle de texte réglable, l’inventaire, l’inspection, la planification, la vérification des journaux et un parcours d’effacement à confirmations multiples. Consultez [le guide de l’interface](ui/README.md).

| Système | Niveau annoncé pour l’interface |
| --- | --- |
| Linux x64 maintenu | AppImage autonome contenant Qt, Python et le moteur C standard. |
| macOS 13 à 26 sur Apple Silicon | Application autonome distribuée dans un DMG. |
| macOS 12 et antérieurs | Non pris en charge par la dépendance graphique distribuée. |
| Windows 10 et 11 x64 | Application autonome distribuée avec un installateur. |
| Windows XP, 7 et 8 | Non pris en charge ; utiliser le média amorçable sur un matériel compatible. |

Les anciens Windows ne disposent plus d’une base système maintenue adaptée à un outil destructif. Leur promettre un effacement client sûr serait trompeur ; le média amorçable est la voie prévue pour les machines qui ne peuvent pas exécuter une plateforme maintenue.

Les paquets autonomes ne requièrent aucune installation préalable de Python ou de PySide6. Le DMG macOS et l’installateur Windows de cette alpha ne sont pas signés par un certificat développeur et ne sont pas notarisés. Les attestations de provenance et les sommes publiées permettent de vérifier la construction, mais elles ne suppriment pas les avertissements de sécurité des systèmes.

Après téléchargement, vérifiez le fichier `.sha256` correspondant. L’AppImage doit ensuite être rendue exécutable, le DMG permet de glisser `datafuckerr.app` dans Applications et l’exécutable Windows installe l’application pour l’utilisateur courant. Il ne faut pas désactiver globalement Gatekeeper, SmartScreen ou l’antivirus pour contourner un avertissement.

Le [générateur de rapport](docs/CLIENT_REPORT.md) transforme un journal accepté par `verify-audit` en PDF lisible. Chaque page porte la mention **RAPPORT TECHNIQUE NON CERTIFIÉ** : le document constitue une trace locale d’exécution, jamais un certificat de non-récupérabilité.

La [recette datafuckerr Live](packaging/live/README.md) et le workflow `ISO graphique` construisent une image Debian Live amd64 BIOS/UEFI contenant la GUI et le binaire standard. Apple Silicon et Secure Boot ne sont pas annoncés. L’image reste expérimentale jusqu’à un test de démarrage matériel indépendant.

## Garanties et limites

diskpurge refuse notamment le disque système détecté, les supports montés détectés, les partitions, les supports en lecture seule, les types inconnus et l’écrasement logique des supports flash. Avant l’exécution, il réinspecte le chemin et compare l’identité et la géométrie avec celles confirmées. Pour un disque physique, deux identités distinctes d’opérateur et de témoin, le fichier d’audit et l’acquittement explicite de la perte de données sont obligatoires.

Ces garde-fous réduisent le risque d’erreur ; ils ne remplacent ni l’autorisation de l’opérateur, ni une sauvegarde, ni l’isolation physique du poste. Le chemin `clear` vérifie l’objet depuis le descripteur ouvert avant la première écriture, mais son usage physique reste désactivé dans le binaire standard tant que l’identité matérielle ne peut pas être reliée de façon suffisante à ce descripteur. Le chemin de purge NVMe ne possède pas encore une revalidation complète de l’identité physique liée à son descripteur ; ces limites et l’absence de qualification bloquent tout usage de production.

Le projet ne promet pas qu’une récupération est « impossible » dans l’absolu. Une affirmation défendable doit désigner une méthode, un support et un niveau de menace, puis s’appuyer sur une qualification, une vérification et une trace d’audit.

Le journal JSON Lines est verrouillé pendant l’écriture, synchronisé et chaîné par SHA-256. La commande verify-audit détecte une troncature, une rupture ou une modification non recalculée. Ce mécanisme ne prouve pas l’auteur du journal : une personne pouvant remplacer tout le fichier peut recalculer toute la chaîne. Pour un usage client, l’empreinte finale doit être ancrée ou signée par un service externe de confiance et conservée séparément.

Le mode destroy décrit une procédure externe. Corrompre un firmware ou rendre un disque apparemment inutilisable ne prouve pas que les données ont été détruites.

## Compilation

Avec Make :

~~~sh
make
make test
~~~

Avec CMake :

~~~sh
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake
~~~

## Utilisation

~~~sh
build/diskpurge list
build/diskpurge verify-audit journal.jsonl
build/diskpurge inspect fichier-test.img
build/diskpurge plan fichier-test.img --method auto
build/diskpurge erase fichier-test.img --confirm IDENTIFIANT_AFFICHÉ --verify full --audit journal.jsonl --operator OPÉRATEUR-DÉMO --witness TÉMOIN-DÉMO
~~~

Cet exemple `erase` est réservé à un nouveau fichier temporaire sans donnée utile ; n’y substituez pas un chemin de périphérique réel. La commande plan ne modifie rien. La commande erase est irréversible. La vérification complète est appliquée par défaut à clear ; sample doit être demandé explicitement et ne fournit qu’un contrôle limité. Lors d’une interruption de clear, le programme tente de synchroniser ce qui a déjà été écrit puis de consigner un échec partiel lorsque l’audit reste accessible ; il ne confirme pas la réussite de cette synchronisation.

Une purge native doit être lancée depuis un environnement de maintenance indépendant. Une commande NVMe Sanitize acceptée peut continuer dans le contrôleur après un redémarrage ou après la perte du suivi par le programme ; interrompre son suivi n’annule pas la commande dans le contrôleur.

## Méthodes

- clear : écriture logique complète, réservée aux disques magnétiques lorsque la politique de l’organisation l’autorise ;
- auto : purge native disponible pour les supports flash, ou écrasement complet pour les HDD ;
- purge : commande native de sanitisation avec contrôle du résultat, actuellement limitée au NVMe direct sous Linux ;
- destroy : destruction physique par un équipement et une procédure qualifiés, non exécutée par diskpurge.

## Gouvernance et assurance

- [Politique de sécurité](SECURITY.md)
- [Guide de contribution](CONTRIBUTING.md)
- [Code de conduite](CODE_OF_CONDUCT.md)
- [Journal des changements](CHANGELOG.md)
- [Modèle de menace](docs/THREAT_MODEL.md)
- [Qualification matérielle](docs/HARDWARE_QUALIFICATION.md)
- [Manuel opérateur](docs/OPERATOR_MANUAL.md)
- [Réponse aux incidents](docs/INCIDENT_RESPONSE.md)
- [Schéma du journal d’audit](docs/AUDIT_SCHEMA.md)
- [Questions fréquentes](docs/FAQ.md)
- [Checklist de publication](docs/RELEASE_CHECKLIST.md)
- [Procédure de publication et vérification des artefacts](docs/RELEASING.md)

Les vulnérabilités ne doivent pas être publiées dans une issue. Suivez le canal privé décrit dans la politique de sécurité.

## Licence

diskpurge est distribué sous licence Apache License 2.0. Consultez [LICENSE](LICENSE).
