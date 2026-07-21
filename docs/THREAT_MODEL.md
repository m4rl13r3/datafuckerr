# Modèle de menace

## Statut du document

Ce modèle couvre le code 0.2.1 présent dans la branche principale. Il doit être revu pour chaque changement de sélection du support, de commande destructive, de vérification, d’audit ou de privilèges.

Le logiciel est une alpha de laboratoire. Les éléments marqués « blocage production » interdisent de le présenter comme un outil d’effacement sécurisé destiné aux clients tant qu’ils ne sont pas corrigés et vérifiés.

## Objectif d’assurance

Pour un support, une méthode et un niveau de menace explicitement définis, diskpurge doit :

1. sélectionner uniquement le support autorisé ;
2. choisir une méthode applicable à sa technologie et à sa portée réelle ;
3. refuser toute ambiguïté avant la première modification ;
4. déterminer sans ambiguïté si l’opération a réussi, échoué ou reste indéterminée ;
5. produire une preuve traçable associée à l’opération réellement exécutée.

La sanitisation vise à rendre l’accès aux données ciblées impraticable pour un niveau d’effort défini. Elle ne constitue pas une promesse absolue contre tout laboratoire, défaut de firmware ou erreur de fabrication.

## Périmètre

Sont inclus :

- l’inventaire et l’inspection des supports ;
- la résolution de la méthode auto ;
- la confirmation de l’identité ;
- l’écrasement logique à zéro et sa vérification ;
- la commande NVMe Sanitize directe sous Linux et le suivi de son état ;
- les messages de résultat et le journal JSON Lines ;
- la compilation, les dépendances et les artefacts distribués.

Sont hors périmètre :

- le système d’exploitation, les pilotes et le firmware du support, considérés comme dépendances non fiables ;
- les outils de destruction physique ;
- la protection des données avant l’opération ;
- la sauvegarde et la restauration ;
- une preuve criminalistique contre l’extraction physique des composants ;
- la réparation, la corruption ou le verrouillage volontaire du firmware.

## Architecture et frontières de confiance

Le flux nominal est :

    opérateur autorisé
            |
            v
    CLI non privilégiée : list, inspect, plan
            |
            v
    identité affichée et confirmation explicite
            |
            v
    CLI privilégiée : revalidation, ouverture exclusive, erase
            |
            v
    API du système et pilote
            |
            v
    contrôleur, pont éventuel et support
            |
            v
    vérification indépendante et preuve d’audit

Les changements de privilèges, l’ouverture du périphérique, un pont USB et toute reconnexion franchissent une frontière de confiance. Le chemin tel que saisi ne suffit pas comme identité : il peut être réattribué entre l’inspection et l’exécution.

## Actifs à protéger

- les données présentes sur tous les supports non ciblés ;
- la confidentialité résiduelle des données du support ciblé ;
- l’intégrité du système en cours d’exécution ;
- l’identité, le consentement et la portée de l’opération ;
- l’exactitude du résultat et de la méthode annoncés ;
- l’intégrité, l’authenticité et la confidentialité des preuves d’audit ;
- les clés, secrets de publication et artefacts de construction ;
- la sécurité physique de l’opérateur et de l’atelier.

## Acteurs et capacités

### Opérateur de bonne foi

Il peut se tromper de chemin, mal lire un identifiant, reconnecter un câble, utiliser un pont incompatible ou interrompre une opération. Les erreurs prévisibles doivent être refusées.

### Processus local hostile

Il peut remplacer un chemin, modifier un fichier, provoquer une reconnexion, falsifier un journal accessible en écriture ou exploiter l’exécution privilégiée.

### Support ou firmware défectueux ou hostile

Il peut annoncer de fausses capacités, ignorer une commande, renvoyer un succès erroné, modifier sa géométrie, masquer des zones ou se comporter différemment après une coupure.

### Adversaire de récupération

Selon le contrat client, il peut employer des outils logiciels ordinaires, un environnement privilégié, des équipements constructeurs ou un laboratoire capable d’accéder aux composants. Le niveau retenu détermine si clear, purge ou destruction physique est acceptable.

### Compromission de la chaîne de construction

Elle peut introduire un binaire différent du code audité, remplacer une dépendance ou falsifier une archive.

## Hypothèses

- l’organisation possède le support et autorise explicitement sa sanitisation ;
- le poste de maintenance est isolé et ne contient aucun support hors périmètre ;
- le binaire et l’environnement de démarrage ont été vérifiés ;
- les horloges et identités d’opérateur sont administrées par la procédure d’atelier ;
- le matériel qualifié correspond exactement au modèle, firmware et transport enregistrés ;
- les standards et documents constructeurs applicables sont disponibles ;
- une méthode cryptographique n’est acceptée que si le chiffrement permanent et la gestion des clés sont établis.

Si une hypothèse ne peut pas être démontrée, l’opération doit être refusée ou escaladée vers une destruction physique qualifiée.

## Menaces, contrôles et écarts

| ID | Menace | Contrôle actuel | Écart et traitement exigé |
| --- | --- | --- | --- |
| T01 | Le chemin désigne un autre support entre inspect et erase | Confirmation textuelle, puis réinspection de l’identifiant, de la capacité et du type avant l’exécution | Blocage production : lier la revalidation au descripteur ouvert exclusivement et revalider aussi montage, lecture seule et portée sans fenêtre concurrente |
| T02 | Un alias, une partition, LVM, dm-crypt, RAID, APFS ou un autre détenteur reste actif | Détection des montages et de certains détenteurs selon la plateforme | Blocage production : graphe complet des dépendances, tests par plateforme et refus de toute ouverture exclusive impossible |
| T03 | Le disque système n’est pas reconnu | Détection partielle du volume racine ; heuristique Windows limitée | Blocage production : aucune exécution Windows réelle et aucune plateforme annoncée sans tests de topologies système |
| T04 | HDD, SSD, NVMe ou flash est mal classé | Refus du type inconnu et de clear sur flash détectée | Croiser plusieurs sources d’identité et refuser les réponses contradictoires |
| T05 | Un pont USB filtre ou transforme les commandes | Aucune purge native via pont n’est annoncée | Qualifier séparément chaque pont et refuser tout transport non inscrit dans la matrice |
| T06 | Une réponse Identify ou un journal natif est tronqué, incohérent ou mal interprété | Tailles fixes, décodage explicite de certains champs | Blocage production : vecteurs normatifs, tests de limites, réponses malformées et comparaison avec un outil indépendant |
| T07 | Sanitize affecte plusieurs espaces de noms ou tout le sous-système | Refus NVMe lorsque le contrôleur annonce un nombre différent de un | Conserver le refus par défaut ; toute portée par espace de noms exige une implémentation et une qualification distinctes |
| T08 | Le contrôleur accepte la commande mais le suivi échoue | L’interruption indique que Sanitize continue et le détail est ajouté à l’audit lorsqu’il reste accessible | L’état externe doit devenir indéterminé plutôt qu’échec, le support être mis en quarantaine et le suivi être repris indépendamment |
| T09 | Coupure, reset, veille ou débranchement pendant l’opération | Les signaux demandent un arrêt contrôlé ; clear synchronise la partie écrite et l’audit décrit l’effacement partiel | Blocage production : machine à états persistante, reprise du journal et essais de panne contrôlés |
| T10 | Une écriture logique laisse des secteurs remappés ou cachés | clear est refusé sur les supports flash détectés | Ne jamais qualifier clear comme purge ; inspecter capacité native et zones cachées selon la technologie |
| T11 | Crypto-erase détruit une clé incomplète, récupérable ou partagée | Préférence actuelle pour crypto-erase lorsqu’annoncé | Blocage production : preuves constructeur, chiffrement permanent, hiérarchie de clés et qualification ; sinon block-erase ou destruction |
| T12 | Le journal est modifié, tronqué ou associé au mauvais support | Audit obligatoire sur disque physique, identifiant d’opération, opérateur, verrouillage, synchronisation, chaîne SHA-256 et verify-audit | Blocage production client : schéma versionné, identité du témoin, résultat brut, signature ou ancrage externe, horodatage fiable et stockage append-only |
| T13 | Le firmware ment sur le succès | Lecture du statut natif | Comparaison indépendante, qualification par révision et politique de destruction lorsque le niveau de menace n’accorde pas confiance au firmware |
| T14 | Une erreur mémoire est exploitée avec les privilèges administrateur | C17, compilation stricte et sanitizers ponctuels | Réduire la durée et le périmètre des privilèges, fuzzing, analyse statique et revue indépendante |
| T15 | Un artefact publié ne correspond pas au code revu | Aucun mécanisme publié | Blocage production : CI protégée, tags signés, sommes, nomenclature des composants et provenance |
| T16 | Une option invalide est interprétée comme une valeur sûre | Les méthodes et modes full/sample sont validés strictement ; full est la valeur par défaut | Maintenir des parseurs fermés et des tests exhaustifs pour toute nouvelle option |
| T17 | Un disque apparemment brické est considéré comme effacé | destroy est refusé par le logiciel | Maintenir la séparation : purge vérifiée, puis destruction physique qualifiée si requise |
| T18 | L’opérateur traite un succès logiciel comme un certificat absolu | Limites documentées | Le rapport doit nommer le niveau clear ou purge, la méthode, la matrice qualifiée et le résultat de vérification |

## Propriétés de sécurité obligatoires

### Identité et autorisation

L’identité destructive doit combiner, lorsque disponibles, numéro de série, modèle, capacité, taille de bloc, identifiants bus et empreinte de topologie. Elle doit être acquise à nouveau depuis le descripteur ouvert pour l’écriture. Une différence, une donnée absente ou une reconnexion invalide la confirmation.

Deux personnes doivent valider le support pour une opération client : l’opérateur et un témoin. Le logiciel doit enregistrer leurs identités sans les déduire du compte administrateur local.

### Portée et méthode

La portée annoncée doit distinguer périphérique logique, namespace, contrôleur et sous-système. auto ne doit sélectionner qu’une méthode au moins aussi forte que la politique demandée. Aucun échec de purge ne doit entraîner un repli silencieux vers clear.

### État terminal

Les seuls états externes sont : refusé, en cours, réussi, échoué et indéterminé. Réussi exige une preuve terminale conforme au standard et à la procédure qualifiée. Une perte de processus, une erreur de journal, un délai dépassé ou une réponse réservée donne indéterminé.

### Vérification

La vérification est propre à la méthode. Pour clear, une lecture indépendante de toute la plage logique est requise lorsque la politique l’impose. Pour une commande native, le statut du contrôleur, la portée et une vérification indépendante doivent être conservés. Lire des zéros après crypto-erase ne démontre pas à lui seul la destruction de la clé.

### Audit

La preuve actuelle inclut un identifiant d’opération dérivé par SHA-256, l’opérateur, la version, l’identité exposée du support, la méthode, la vérification, les états et une chaîne d’empreintes. Pour la production, elle doit aussi inclure un identifiant aléatoire généré par une source cryptographique, le témoin, l’empreinte du binaire, l’identité après opération, le transport, les réponses natives brutes nécessaires et un verdict indéterminé explicite.

Le chaînage détecte une altération accidentelle ou une modification qui ne recalcule pas les empreintes suivantes. Il n’apporte pas d’authenticité : un acteur ayant accès à tout le fichier peut le réécrire et recalculer une chaîne valide. L’empreinte finale doit être signée ou ancrée dans un système externe de confiance avant que le journal ne serve de preuve client.

## Risques résiduels

Même après qualification :

- un défaut inédit de firmware peut invalider une commande ;
- les blocs défectueux ou zones constructeurs peuvent échapper à l’observation de l’hôte ;
- une méthode cryptographique dépend de la conception et de l’implémentation des clés ;
- un laboratoire physique peut dépasser le niveau de menace prévu ;
- l’audit prouve une procédure observée, pas une impossibilité mathématique de récupération ;
- la destruction physique demeure nécessaire lorsque la politique n’accorde pas confiance au contrôleur.

## Critères de sortie de l’alpha

Tous les blocages production du tableau doivent être fermés, testés et revus. La matrice de qualification doit être publique, les plateformes non qualifiées refusées à l’exécution et un audit externe du chemin destructif doit être achevé. La [checklist de publication](RELEASE_CHECKLIST.md) constitue la décision finale.

## Références

- [NIST SP 800-88 Rev. 2 — Guidelines for Media Sanitization](https://csrc.nist.gov/pubs/sp/800/88/r2/final), septembre 2025.
- [NVM Express Base Specification, révision 2.3](https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.3-2025.08.01-Ratified.pdf), notamment la commande et le journal Sanitize.
- [Procédure de qualification matérielle de diskpurge](HARDWARE_QUALIFICATION.md).
