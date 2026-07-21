# Qualification matérielle

## Objet

Cette procédure détermine si une version précise de diskpurge peut appliquer une méthode précise à un matériel précis dans un environnement précis. Elle ne qualifie jamais une marque entière ni un type générique de connecteur.

Une qualification est valable uniquement pour le tuple :

    version et empreinte de diskpurge
    + système, noyau et pilote
    + modèle, révision matérielle et firmware du support
    + capacité et taille de bloc
    + contrôleur et topologie
    + pont, boîtier, câble et alimentation éventuels
    + méthode et paramètres

Tout changement d’un élément du tuple suspend la qualification jusqu’à une analyse documentée ou un nouvel essai.

## Principes

- Tous les supports d’essai sont sacrifiables, propriété du laboratoire et exempts de données réelles.
- Tous les autres supports sont physiquement débranchés.
- La station démarre depuis un environnement de maintenance vérifié.
- L’opérateur, le témoin et la personne qui approuve le rapport sont identifiés.
- La qualification distingue clear, purge et destruction physique.
- Un succès déclaré par le firmware n’est pas la seule preuve.
- Un résultat ambigu est un échec de qualification.
- La vitesse ne peut jamais réduire un contrôle de sécurité.

## Rôles

**Responsable de qualification** : approuve le protocole, le niveau de menace et les critères avant les essais.

**Opérateur** : prépare le support et exécute les scénarios.

**Témoin** : vérifie physiquement l’identité, le câblage, la confirmation et le résultat.

**Relecteur indépendant** : examine les preuves et prononce le verdict. Il ne doit pas être l’auteur unique du backend évalué.

## Sécurité du laboratoire

1. Réaliser les essais dans une zone contrôlée, sans données client.
2. Étiqueter chaque support avec un identifiant de laboratoire sans réutiliser son numéro de série comme seule référence.
3. Débrancher physiquement les supports hors périmètre et photographier la topologie autorisée.
4. Utiliser une alimentation adaptée et empêcher veille, hibernation et redémarrage automatique.
5. Prévoir un moyen indépendant de relire les journaux natifs.
6. Placer tout support dont l’état devient indéterminé en quarantaine ; ne pas le réutiliser.
7. Effectuer les essais de coupure uniquement dans la phase dédiée, après un premier succès nominal, sur le support sacrifiable et avec une procédure de sécurité approuvée.

## Dossier de preuve

Chaque campagne possède un identifiant unique et conserve :

- objectif, méthode, niveau de menace et critères d’acceptation approuvés ;
- version, commit et empreinte cryptographique du binaire ;
- commandes de construction, compilateur, options et dépendances ;
- système, noyau, pilote, architecture et configuration de démarrage ;
- modèle, numéro de série expurgé dans le rapport public, firmware, capacité et blocs ;
- identité du contrôleur, du pont, du boîtier, du câble et de l’alimentation ;
- sorties brutes d’identification avant et après ;
- captures des états de montage et du graphe de dépendances ;
- données de test, carte des emplacements et empreintes attendues ;
- sortie complète de diskpurge, code de retour et audit ;
- résultat de verify-audit et empreinte finale ancrée ou signée hors de la station ;
- journaux du noyau et réponses natives brutes ;
- résultats de l’outil indépendant ;
- anomalies, photographies utiles, verdict et signatures des trois rôles.

Les originaux sont stockés dans un espace append-only avec contrôle d’accès. Le rapport public retire les identifiants sensibles mais conserve assez d’informations pour distinguer le matériel qualifié.

## Niveaux de qualification

| Niveau | Contenu | Autorisation |
| --- | --- | --- |
| Q0 | Construction, tests unitaires, réponses simulées, fuzzing | Aucun périphérique réel |
| Q1 | Essai nominal sur au moins trois exemplaires sacrifiables du tuple | Laboratoire interne uniquement |
| Q2 | Essais négatifs, interruption et répétabilité, revue indépendante | Pilote contrôlé |
| Q3 | Matrice représentative, audit externe et procédure d’atelier validée | Usage client limité au tuple publié |

Une version de production destinée aux clients exige Q3. « Compatible NVMe », « compatible USB » ou « compatible Windows » ne sont pas des qualifications acceptables.

## Préconditions communes

Avant chaque essai :

- vérifier l’autorisation écrite de détruire le support ;
- vérifier l’empreinte du binaire et l’intégrité de l’environnement ;
- confirmer que le support est le seul support de données connecté ;
- collecter l’identité par diskpurge et par un outil indépendant ;
- comparer chemin, modèle, capacité, taille de bloc, série, transport et topologie ;
- vérifier l’absence de montage, swap, RAID, LVM, dm-crypt, APFS, Storage Spaces ou autre détenteur ;
- vérifier la lecture seule, les réservations, le verrouillage de sécurité et les erreurs de santé ;
- enregistrer les capacités natives, la portée de la commande et les zones cachées connues ;
- refuser toute information absente, contradictoire ou instable.

## Préparation des données d’essai

Le support est rempli sur toute sa plage logique avec un jeu synthétique unique à la campagne. Il doit contenir :

- un en-tête reconnaissable au début et à la fin de chaque région définie ;
- des marqueurs différents aux limites de partitions, au milieu et aux derniers blocs ;
- une séquence non compressible et une séquence structurée ;
- un système de fichiers de test avec de petits et grands fichiers ;
- une table des emplacements et empreintes enregistrée hors du support.

Les données ne doivent ressembler à aucune donnée client. Après écriture, une relecture intégrale confirme que les marqueurs attendus sont réellement présents avant la sanitisation.

## Essai nominal commun

1. Exécuter list, inspect et plan sans privilèges destructifs.
2. Comparer l’identité et la méthode proposées avec le dossier approuvé.
3. Déconnecter puis reconnecter le même support ; vérifier que le changement éventuel de chemin n’altère pas l’identité.
4. Remplacer le support par un autre exemplaire ; vérifier que l’ancienne confirmation est refusée.
5. Ouvrir l’exécution avec les privilèges minimaux.
6. Faire vérifier visuellement l’étiquette, le chemin et l’identifiant par le témoin.
7. Lancer la méthode qualifiée avec opérateur, acquittement, audit et vérification prescrite.
8. Conserver la sortie, le code de retour, les horodatages et les journaux.
9. Exécuter verify-audit, puis ancrer ou signer l’empreinte finale par un système externe.
10. Réinspecter le support sans y écrire.
11. Exécuter la vérification indépendante et prononcer réussi, échoué ou indéterminé.

## Qualification de clear sur HDD

clear est une écriture logique, pas une purge physique.

Critères supplémentaires :

- la technologie rotative est confirmée par plusieurs sources ;
- la capacité présentée correspond à la capacité native attendue ;
- aucune zone masquée ou réduction de capacité n’est inexpliquée ;
- une synchronisation physique réussit ;
- une relecture indépendante de la totalité de la plage logique ne trouve aucun ancien marqueur et confirme le motif attendu ;
- la taille relue correspond exactement à la taille inspectée avant l’opération ;
- les erreurs d’écriture ou de lecture entraînent un échec et une orientation vers destruction physique.

Les secteurs réalloués restent un risque résiduel. Le rapport doit annoncer clear et ne jamais employer le mot purge pour ce résultat.

## Qualification de NVMe Sanitize

Le backend actuel vise un sous-système NVMe direct sous Linux et refuse un contrôleur annonçant plusieurs espaces de noms. Le protocole enregistre la révision de la spécification réellement implémentée.

Critères supplémentaires :

- l’identité du contrôleur et le nombre d’espaces de noms sont confirmés indépendamment ;
- les bits de capacité Sanitize et l’action choisie correspondent aux données Identify brutes ;
- l’étendue réelle de la commande est compatible avec le support physiquement isolé ;
- l’état antérieur, le démarrage, la progression et l’état terminal du journal Sanitize sont conservés ;
- le statut terminal est relu par un outil indépendant après réouverture et après un redémarrage contrôlé ;
- tous les anciens marqueurs logiques sont absents de chaque espace adressable ;
- les erreurs du noyau et du contrôleur sont absentes ou expliquées ;
- trois cycles nominaux réussissent sur chacun d’au moins trois exemplaires.

Pour crypto-erase, ajouter :

- preuve constructeur que toutes les données utilisateur, métadonnées et caches concernés sont toujours chiffrés ;
- description de la hiérarchie, de la génération et de la portée des clés ;
- preuve que l’action remplace ou détruit toutes les clés nécessaires ;
- absence de clé externe ou de copie persistante permettant le déchiffrement ;
- justification indépendante du niveau de confiance accordé au constructeur.

Lire des zéros ou des données déallouées après crypto-erase ne prouve pas la destruction des clés. Si les exigences cryptographiques ne sont pas démontrables, crypto-erase est refusé et une autre méthode qualifiée est requise.

Pour block-erase, confirmer que l’implémentation constructeur couvre les zones utilisateur non allouées, surprovisionnées et les caches visés par le standard applicable. Une simple lecture de la plage LBA ne suffit pas à établir cette couverture.

## Ponts USB et boîtiers

Chaque combinaison pont, firmware, boîtier, câble et support constitue un tuple distinct.

La qualification vérifie :

- la stabilité de l’identité exposée ;
- la transmission exacte de chaque commande et de chaque statut ;
- l’absence de traduction vers une commande moins forte ;
- la portée réelle côté support ;
- le comportement après déconnexion et reconnexion ;
- le comportement en cas d’alimentation insuffisante.

Si une commande native ou son journal n’est pas transmis intégralement, le pont est non qualifié. USB-C décrit un connecteur, pas un protocole ni une garantie de sanitisation.

## Scénarios négatifs obligatoires

Chaque scénario doit refuser avant la première écriture, sauf ceux explicitement consacrés à la reprise :

- confirmation absente, ancienne, tronquée ou appartenant à un autre exemplaire ;
- chemin réattribué entre inspection et exécution ;
- disque système direct, partition du système et support portant une couche de stockage du système ;
- partition montée, swap actif, RAID, volume logique, conteneur chiffré ou namespace partagé ;
- support passé en lecture seule ;
- type inconnu ou sources de type contradictoires ;
- capacité ou taille de bloc modifiée ;
- option ou valeur inconnue ;
- méthode clear demandée sur flash ;
- purge demandée sans capacité correspondante ;
- plusieurs espaces de noms pour le backend actuel ;
- réponse Identify tronquée, réservée ou incohérente ;
- statut Sanitize réservé, échec ou absence persistante de démarrage ;
- journal d’audit inaccessible avant la commande ;
- perte des privilèges ou ouverture exclusive impossible.

La preuve doit montrer que le contenu du support est inchangé après chaque refus.

## Essais de reprise

Ces essais ont lieu seulement au niveau Q2, dans le laboratoire isolé :

- arrêt du processus après acceptation de la commande ;
- redémarrage contrôlé du système ;
- coupure d’alimentation du support selon un plan approuvé ;
- perte temporaire du journal ou du descripteur ;
- reconnexion sur un chemin différent ;
- retour d’un état terminal après expiration du délai logiciel.

Le résultat attendu par défaut est indéterminé et mise en quarantaine. La qualification n’accepte un retour à réussi qu’après reprise indépendante, identité revalidée et statut terminal normatif. Aucun scénario ne doit transformer une perte de suivi en échec supposé permettant la réutilisation immédiate du support.

## Vérification indépendante

L’outil indépendant ne partage ni le parseur ni la logique de décision de diskpurge. Il confirme l’identité, les capacités, la portée, l’état terminal et la disparition des marqueurs adressables.

Pour un niveau de menace incluant un laboratoire de récupération, une analyse par un laboratoire spécialisé et indépendant est nécessaire. Si cette analyse n’est pas réalisable ou concluante, le support est détruit physiquement par une filière qualifiée.

## Critères d’acceptation Q3

Toutes les conditions suivantes sont obligatoires :

- aucun effacement du mauvais support sur toute la campagne ;
- aucun scénario négatif ne modifie le support ;
- cent pour cent des essais nominaux et de reprise produisent l’état attendu ;
- aucun crash, erreur sanitizer ou diagnostic critique d’analyse statique ;
- concordance entre diskpurge et les outils indépendants ;
- preuves complètes et vérifiables pour chaque exemplaire ;
- risques résiduels acceptés par écrit pour le niveau de menace ;
- revue du rapport par une personne indépendante ;
- entrée publiée dans la matrice qualifiée ;
- procédure de retrait rapide si une anomalie apparaît après publication.

Une seule ambiguïté critique invalide le tuple. Elle ne peut pas être compensée par une majorité d’essais réussis.

## Matrice publiée

| Version diskpurge | Système | Support et firmware | Transport | Méthode | Niveau | Verdict | Rapport |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0.2.2 | Tous | Tous | Tous | Toutes | Q0 | Non qualifié pour périphérique réel | À produire |

Cette ligne est la seule autorisée tant qu’aucune campagne Q1 à Q3 n’est achevée.

## Suspension et renouvellement

La qualification est immédiatement suspendue après :

- mise à jour de diskpurge, du noyau, du pilote ou du firmware ;
- révision matérielle ou changement de fournisseur de composants ;
- nouveau pont, câble, boîtier ou alimentation ;
- anomalie client, divergence de journal ou support récupérable ;
- changement du standard ou découverte d’un erratum applicable ;
- expiration de la période de revue fixée à douze mois maximum.

Le responsable décide si une analyse différentielle suffit ou si la campagne complète doit être répétée. Toute décision est jointe au dossier de preuve.

## Modèle de verdict

Le rapport final indique :

    Identifiant de campagne :
    Version et empreinte du binaire :
    Tuple matériel et logiciel :
    Méthode et portée :
    Niveau de menace :
    Scénarios exécutés :
    Anomalies :
    Risques résiduels :
    Niveau atteint :
    Verdict : qualifié, non qualifié ou suspendu
    Limites d’usage :
    Date d’expiration :
    Opérateur :
    Témoin :
    Relecteur indépendant :

Le mot « certifié » est interdit sauf certification formelle délivrée par une autorité identifiée et pour un périmètre explicitement décrit.

## Références

- [NIST SP 800-88 Rev. 2 — Guidelines for Media Sanitization](https://csrc.nist.gov/pubs/sp/800/88/r2/final).
- [NVM Express Base Specification, révision 2.3](https://nvmexpress.org/wp-content/uploads/NVM-Express-Base-Specification-Revision-2.3-2025.08.01-Ratified.pdf).
- [Modèle de menace de diskpurge](THREAT_MODEL.md).
- [Checklist de publication](RELEASE_CHECKLIST.md).
