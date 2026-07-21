# Checklist de publication

## Règle de décision

Cette checklist est une barrière de sûreté, pas un aide-mémoire facultatif. Pour une publication destinée aux clients, toutes les cases P0 doivent être cochées avec un lien vers une preuve. Une dérogation P0 est interdite.

Les cases P1 peuvent uniquement faire l’objet d’une acceptation de risque écrite, limitée dans le temps et approuvée par la sécurité, la direction produit et le responsable juridique. Une date commerciale ne constitue pas une justification.

## Types de publication

| Type | Usage autorisé | Exigence |
| --- | --- | --- |
| Alpha | Fichiers virtuels et développement | Gouvernance, tests non destructifs et avertissements |
| Laboratoire | Supports sacrifiables isolés | Q1 minimum pour chaque tuple |
| Pilote | Atelier interne contrôlé, aucune promesse générale | Q2, procédure et supervision |
| Production client | Uniquement les tuples publiés | Q3 et toutes les barrières P0 |

Une plateforme non qualifiée doit refuser erase sur un périphérique réel. La simple mention « expérimentale » ne suffit pas à sécuriser un chemin exécutable.

## 1. Portée et affirmation

- [ ] **P0** La version, le commit, les plateformes et les méthodes proposées sont figés.
- [ ] **P0** Une source unique fournit la version au binaire, aux systèmes de construction et aux documents.
- [ ] **P0** La matrice publique distingue système, modèle, firmware, transport, méthode et niveau de qualification.
- [ ] **P0** Chaque affirmation commerciale correspond à une preuve Q3.
- [x] **P0** Les termes clear, purge et destruction physique sont employés sans ambiguïté.
- [x] **P0** Aucune promesse « impossible à récupérer », « universel », « tous disques » ou « certifié » n’est formulée sans périmètre et autorité.
- [ ] **P1** Les objectifs de performance sont mesurés en débit soutenu et accompagnés du matériel utilisé.

Preuves :

    Version :
    Commit :
    Périmètre :
    Matrice :
    Revue des affirmations : README.md, docs/OPERATOR_MANUAL.md et docs/FAQ.md

## 2. Gouvernance et juridique

- [x] **P0** Le dépôt contient l’Apache License 2.0 complète.
- [x] **P0** SECURITY.md, CONTRIBUTING.md et CODE_OF_CONDUCT.md existent.
- [x] **P0** Le modèle de menace et le protocole de qualification existent.
- [ ] **P0** Un titulaire ou une politique de gestion du copyright est identifié.
- [ ] **P0** Le canal privé de sécurité est activé, testé et surveillé par au moins deux personnes.
- [ ] **P0** Les responsabilités de maintenance, d’approbation et de publication sont attribuées.
- [ ] **P0** Une revue des licences et attributions de toutes les dépendances est archivée.
- [ ] **P0** Les conditions de service client, responsabilités, conservation des preuves et exigences réglementaires ont été revues par une personne compétente.
- [ ] **P1** La politique de fin de support et la durée des correctifs sont publiées.

Preuves :

    Responsables :
    Test du canal privé :
    Revue juridique :
    Inventaire des licences :

## 3. Conception sûre

- [ ] **P0** Tous les écarts marqués « blocage production » dans le modèle de menace sont fermés.
- [ ] **P0** Le support est ouvert exclusivement et son identité est revalidée depuis le descripteur juste avant la première modification.
- [ ] **P0** Toute reconnexion, différence de géométrie, identité partielle ou réponse contradictoire annule la confirmation.
- [ ] **P0** La détection couvre le disque système et le graphe complet des détenteurs pour chaque plateforme annoncée.
- [ ] **P0** Le programme refuse montage, swap, RAID, volume logique, chiffrement, espace de noms partagé, lecture seule et portée indéterminée.
- [ ] **P0** Les options et valeurs inconnues sont rejetées ; aucun choix de sécurité ne repose sur une valeur implicite ambiguë.
- [ ] **P0** auto ne se replie jamais silencieusement vers une méthode plus faible.
- [x] **P0** Une commande acceptée mais non suivie aboutit à indéterminé et à une procédure de quarantaine.
- [ ] **P0** Les privilèges sont minimisés et séparés autant que le permet la plateforme.
- [x] **P0** Aucun mode de corruption de firmware, de verrouillage malveillant ou de bricking n’est présent.
- [ ] **P1** Une estimation de durée et un mécanisme d’annulation sûre sont définis pour chaque état où l’annulation est possible.

Preuves :

    Revue d’architecture :
    Tests de sélection :
    Tests de reprise : tests/test_purge.c et docs/INCIDENT_RESPONSE.md
    Revue des privilèges :

## 4. Qualité du code

- [ ] **P0** Deux personnes ont revu chaque chemin destructif, dont une qui n’en est pas l’auteur.
- [ ] **P0** La construction C17 stricte ne produit aucun avertissement sur les compilateurs pris en charge.
- [ ] **P0** Les analyses statiques convenues ne signalent aucun défaut critique ou élevé non résolu.
- [ ] **P0** AddressSanitizer et UndefinedBehaviorSanitizer passent sur tous les chemins simulables.
- [ ] **P0** Le parseur CLI, les réponses natives, les journaux et les calculs de taille sont fuzzés avec un corpus archivé.
- [ ] **P0** Les additions, multiplications, conversions de tailles, alignements et limites de périphérique ont des tests de débordement.
- [ ] **P0** Les erreurs système et statuts réservés sont injectés et conduisent à un refus ou à indéterminé.
- [ ] **P0** Aucun secret, numéro de série réel, image de disque ou donnée client n’existe dans le dépôt ou son historique.
- [ ] **P1** La couverture des branches critiques est mesurée et les exclusions sont justifiées.

Preuves :

    Revues :
    Compilateurs :
    Analyses :
    Sanitizers :
    Fuzzing :
    Couverture :

## 5. Tests automatiques

- [ ] **P0** La CI s’exécute sur Linux, macOS et Windows pour toutes les plateformes annoncées.
- [ ] **P0** Les tests automatiques sont techniquement incapables d’ouvrir un périphérique bloc réel en écriture.
- [ ] **P0** Une abstraction ou simulation des appels natifs couvre les succès, échecs, délais, réponses malformées et pertes de suivi.
- [ ] **P0** Les tests prouvent qu’un mauvais identifiant, un identifiant périmé et un chemin réattribué ne modifient rien.
- [ ] **P0** Les tests couvrent les fichiers vides, tailles non alignées, très grandes tailles et écritures partielles.
- [ ] **P0** Chaque garde-fou possède un test positif et un test négatif.
- [ ] **P0** Les tests de compatibilité entre versions du schéma d’audit passent.
- [ ] **P1** Les performances et la consommation mémoire font l’objet de seuils de non-régression.

Preuves :

    CI :
    Simulation :
    Garde-fous :
    Audit :
    Performances :

## 6. Qualification des plateformes

Pour chaque plateforme annoncée :

- [ ] **P0** L’API d’identité et la notion de disque entier sont documentées.
- [ ] **P0** La protection du disque système est testée sur les topologies prises en charge.
- [ ] **P0** Les montages indirects et gestionnaires de volumes sont testés.
- [ ] **P0** L’ouverture exclusive et les privilèges minimaux sont vérifiés.
- [ ] **P0** Les commandes natives sont conformes à la révision normative déclarée.
- [ ] **P0** Les codes de retour et états terminal/indéterminé sont mappés sans ambiguïté.
- [ ] **P0** La reprise après redémarrage et perte de processus est testée.
- [x] **P0** Les plateformes non prêtes sont désactivées à l’exécution sur périphérique réel.

Preuves Linux :

    Distribution et noyau :
    Topologies :
    API et normes :
    Rapport :

Preuves macOS :

    Version :
    Topologies :
    API et normes :
    Rapport :

Preuves Windows :

    Version :
    Topologies :
    API et normes :
    Rapport :

## 7. Qualification matérielle

- [ ] **P0** La procédure de qualification a été exécutée sans adaptation non approuvée.
- [ ] **P0** Au moins trois exemplaires de chaque tuple ont réussi trois cycles nominaux.
- [ ] **P0** Tous les scénarios négatifs ont refusé avant modification.
- [ ] **P0** Les scénarios de reprise et coupure contrôlée produisent le verdict attendu.
- [ ] **P0** Un outil indépendant confirme identité, capacités, portée et état terminal.
- [ ] **P0** Les hypothèses de crypto-erase sont démontrées ou cette méthode est désactivée.
- [ ] **P0** Les ponts et boîtiers sont qualifiés séparément ou explicitement refusés.
- [ ] **P0** Un relecteur indépendant approuve les dossiers de preuve.
- [ ] **P0** La matrice publique, les limites et la date d’expiration sont à jour.
- [ ] **P1** Un laboratoire externe a tenté une récupération pour les niveaux de menace qui l’exigent.

Preuves :

    Campagnes :
    Outils indépendants :
    Revue :
    Matrice :

## 8. Audit et certificat

- [x] **Acquis alpha** Le journal est verrouillé, synchronisé, chaîné par SHA-256 et vérifiable avec verify-audit.
- [x] **Acquis alpha** Un identifiant d’opération et l’identifiant d’opérateur sont consignés ; l’audit est obligatoire sur disque physique.
- [x] **P0** Le schéma d’audit est versionné et documenté.
- [x] **P0** Chaque opération possède un identifiant aléatoire unique.
- [x] **P0** Les identités du support avant et après, de l’opérateur et du témoin sont enregistrées.
- [ ] **P0** Le transport, la portée, la méthode demandée et exécutée, les statuts bruts et la vérification sont enregistrés.
- [x] **P0** Le journal distingue refusé, en cours, réussi, échoué et indéterminé.
- [ ] **P0** L’empreinte finale de la chaîne est signée ou ancrée par un service externe et transférée vers un stockage append-only.
- [ ] **P0** L’horodatage fiable et les dérives d’horloge sont traités.
- [x] **P0** Une interruption avant l’écriture de l’audit empêche le démarrage de la commande.
- [ ] **P0** Le certificat ne peut être émis qu’après validation de toutes les preuves exigées.
- [ ] **P0** La vérification hors ligne d’un certificat est testée.
- [ ] **P1** La politique de rétention et d’effacement des preuves respecte les obligations client.

Preuves :

    Schéma : docs/AUDIT_SCHEMA.md ; tests/test_audit.c
    Signature :
    Stockage :
    Vérificateur :
    Rétention :

## 9. Chaîne de construction et distribution

- [ ] **P0** Les dépendances et versions sont verrouillées ou leur absence est démontrée.
- [ ] **P0** La construction s’effectue dans une CI protégée depuis un tag revu.
- [ ] **P0** Le tag et les artefacts sont signés par une identité de publication protégée.
- [ ] **P0** Des sommes SHA-256 sont publiées par un canal distinct ou signé.
- [ ] **P0** Une nomenclature logicielle au format standard est jointe.
- [ ] **P0** Une attestation de provenance relie source, environnement et artefact.
- [ ] **P0** Les archives ne contiennent que les fichiers prévus, la licence et les attributions.
- [ ] **P0** L’installation et la désinstallation sont testées sans élargissement permanent des privilèges.
- [ ] **P1** Les constructions sont reproductibles ou les différences sont expliquées et contrôlées.

Preuves :

    CI de publication : .github/workflows/release.yml
    Tag :
    Signatures et sommes : outils de génération et de contrôle dans tools/release ; preuve d’exécution à joindre
    Nomenclature : CycloneDX 1.5 générée par tools/release/package_release.py ; SBOM de chaque version à joindre
    Provenance : attestation GitHub configurée ; URL et vérification de chaque attestation à joindre
    Reproductibilité :

## 10. Documentation et expérience opérateur

- [x] **P0** Le README affiche clairement le statut alpha et les limites actuelles.
- [x] **P0** Le manuel décrit les préconditions, refus, états, reprises et conséquences irréversibles.
- [x] **P0** Le guide d’atelier impose l’isolation physique, le double contrôle et la quarantaine.
- [x] **P0** La documentation donne un exemple de chaque état sans utiliser de support réel.
- [ ] **P0** Les messages localisés conservent le même niveau de gravité et les mêmes garde-fous.
- [x] **P0** Les limites du connecteur USB-C et des ponts sont visibles avant l’exécution.
- [x] **P0** La différence entre vérification logique et preuve de purge native est explicite.
- [x] **P0** La FAQ refuse le bricking comme méthode de sanitisation.
- [ ] **P1** Le mode plan fournit une estimation de durée et la justification de la méthode.

Preuves :

    Manuel : docs/OPERATOR_MANUAL.md
    Guide d’atelier : docs/OPERATOR_MANUAL.md et docs/INCIDENT_RESPONSE.md
    Revue terminologique : README.md, docs/OPERATOR_MANUAL.md, docs/AUDIT_SCHEMA.md et docs/FAQ.md
    Tests utilisateurs :

## 11. Exploitation et réponse aux incidents

- [x] **P0** Une procédure traite panne, coupure, perte de suivi, disque disparu et audit incomplet.
- [ ] **P0** Une zone de quarantaine physique et un registre existent.
- [ ] **P0** Les opérateurs savent qu’une commande native peut continuer après l’arrêt du processus.
- [ ] **P0** Une procédure de retrait d’un tuple qualifié peut être exécutée immédiatement.
- [ ] **P0** Le canal de vulnérabilité, l’astreinte et les contacts clients ont été testés.
- [ ] **P0** Un exercice de réponse à un effacement du mauvais support a été mené sans données réelles.
- [ ] **P0** Un exercice de fausse attestation ou de récupération après succès a été mené.
- [ ] **P1** Les métriques de refus, d’échec et d’état indéterminé sont suivies sans collecter de données sensibles.

Preuves :

    Procédures : docs/INCIDENT_RESPONSE.md
    Exercice :
    Retrait de qualification :
    Contacts :

## 12. Revue finale

- [ ] Le changelog contient les fonctionnalités, correctifs de sécurité et incompatibilités.
- [ ] Les problèmes ouverts ont été triés ; aucun P0 ou P1 non accepté ne reste.
- [ ] La candidate a été gelée et testée sans changement après qualification.
- [ ] Les notes de publication citent précisément les tuples qualifiés et ceux qui sont refusés.
- [ ] Les personnes signataires ont vérifié les preuves, pas seulement les cases.

Signatures :

    Responsable technique :
    Responsable sécurité :
    Responsable qualification :
    Responsable exploitation :
    Responsable produit :
    Responsable juridique :
    Date et identifiant de décision :
    Verdict : publier, refuser ou limiter au laboratoire

## État de la version 0.2.3

La version 0.2.3 reste une alpha. Elle possède désormais une vérification complète par défaut, une validation stricte de full/sample, une réinspection de l’identité, un acquittement explicite et un audit chaîné vérifiable. Elle ne satisfait notamment pas les barrières P0 relatives à une revalidation liée au descripteur ouvert, à la protection Windows, à la qualification matérielle, à la reprise persistante après interruption, à l’authenticité externe de l’audit et à la chaîne de publication.

Le prochain jalon honnête est une préversion de développement destinée aux fichiers virtuels. Un usage client ne devient autorisé qu’après preuve Q3 et approbation finale de cette checklist.
