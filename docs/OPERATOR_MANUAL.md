# Manuel opérateur

## Décision d’emploi

diskpurge 0.2.2 est une alpha destinée aux fichiers virtuels et au développement. Le binaire standard ne contient actuellement aucun tuple matériel réel qualifié. Il refuse donc l’effacement de tout disque physique, même si les autres contrôles sont satisfaits.

Cette restriction est une barrière de sûreté volontaire. Ne l’interprétez pas comme une erreur à contourner et n’utilisez pas cette version sur un support client.

Un binaire compilé avec le mode laboratoire affiche une version terminée par `-lab`. Son option `--lab-mode` ne sert qu’à qualifier des supports sacrifiables dans un laboratoire isolé. Ce binaire ne doit pas être distribué aux opérateurs de production, employé sur des données réelles ni servir à produire un certificat client.

La destruction physique reste hors du logiciel. Aucune commande de bricking, de corruption du firmware ou de verrouillage malveillant n’est implémentée, et la méthode `destroy` est refusée par `erase`.

## Responsabilités

Toute opération sur un support physique exige deux personnes différentes :

- l’opérateur exécute la procédure et répond de la cible sélectionnée ;
- le témoin contrôle indépendamment l’autorisation, l’étiquette physique, l’identité affichée, la méthode et la destination du journal.

Les valeurs de `--operator` et `--witness` sont consignées mais ne constituent pas une authentification. L’organisation doit relier ces identifiants à des personnes habilitées et conserver la preuve de leur contrôle. Le programme exige des valeurs distinctes sur un disque physique.

## Préconditions d’un futur atelier qualifié

Ces préconditions ne rendent pas la version actuelle apte à la production. Elles décrivent la barrière minimale à ajouter à une qualification Q3 publiée :

1. Démarrer une station de maintenance indépendante qui ne s’exécute pas depuis la cible.
2. Déconnecter physiquement tous les supports qui ne participent pas à l’opération.
3. Étiqueter la cible et faire rapprocher par deux personnes le modèle, le numéro stable, la capacité, le firmware, le transport et le port utilisé.
4. Stocker le journal sur un support distinct de la cible, protégé et exportable vers un stockage en ajout seul.
5. Désactiver la veille et garantir une alimentation stable pendant toute commande native.
6. Vérifier la version du binaire. Une version contenant `-lab` interdit l’usage client.
7. Vérifier que le tuple exact figure dans la matrice de qualification publiée et non expirée. Le tuple comprend au minimum le système, le modèle, le firmware, le transport, l’environnement, la topologie, le type de support, la méthode résolue, la sous-méthode native, la capacité, la taille de bloc et le mode de vérification.
8. Obtenir l’autorisation d’effacement et confirmer la politique de conservation des preuves.

USB-C désigne un connecteur, pas une méthode de stockage. Un boîtier ou un pont USB peut filtrer une commande native, présenter une autre identité ou modifier la portée. Chaque combinaison boîtier, câble, pont et disque doit être qualifiée comme un transport distinct.

## Commandes sans modification

Les commandes suivantes sont en lecture seule :

~~~sh
build/diskpurge --version
build/diskpurge list
build/diskpurge inspect CHEMIN
build/diskpurge plan CHEMIN --method auto
build/diskpurge verify-audit journal.jsonl
~~~

`plan` ne modifie pas la cible. Un code de sortie 3 signifie que le plan est indisponible ou non exécutable. Dans le binaire standard actuel, un plan visant un disque physique doit notamment être refusé faute de tuple réel qualifié.

## Démonstration autorisée sur fichier virtuel

Cette démonstration crée un répertoire temporaire unique et un nouveau fichier sans donnée utile. Elle n’emploie ni périphérique bloc ni mode laboratoire.

Sous Linux ou macOS :

~~~sh
répertoire=$(mktemp -d "${TMPDIR:-/tmp}/diskpurge-demo.XXXXXX")
fichier="$répertoire/essai.img"
journal="$répertoire/essai.audit.jsonl"
dd if=/dev/zero of="$fichier" bs=1048576 count=64
build/diskpurge inspect "$fichier"
build/diskpurge plan "$fichier" --method clear --verify full
build/diskpurge erase "$fichier" --confirm IDENTIFIANT_AFFICHÉ --method clear --verify full --audit "$journal" --operator opérateur-démo --witness témoin-démo
build/diskpurge verify-audit "$journal"
~~~

Sous Windows PowerShell :

~~~powershell
$répertoire = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid())
New-Item -ItemType Directory -Path $répertoire | Out-Null
$fichier = Join-Path $répertoire "essai.img"
$journal = Join-Path $répertoire "essai.audit.jsonl"
$flux = [System.IO.File]::Create($fichier)
$flux.SetLength(64MB)
$flux.Dispose()
& .\build\diskpurge.exe inspect $fichier
& .\build\diskpurge.exe plan $fichier --method clear --verify full
& .\build\diskpurge.exe erase $fichier --confirm IDENTIFIANT_AFFICHÉ --method clear --verify full --audit $journal --operator opérateur-démo --witness témoin-démo
& .\build\diskpurge.exe verify-audit $journal
~~~

Remplacez `IDENTIFIANT_AFFICHÉ` par la valeur exacte donnée par `inspect`. N’utilisez jamais un chemin de disque réel pour reproduire cet exemple.

## Méthodes

| Valeur CLI | Effet prévu | Limite essentielle |
| --- | --- | --- |
| `auto` | Choisit `clear-zero` pour un HDD ou un fichier et une purge native disponible pour un support flash. | Ne rend pas une cible non qualifiée exécutable et ne doit pas se rabattre silencieusement vers une méthode plus faible. |
| `clear` | Écrit des zéros sur toute la portée logique puis relit complètement par défaut. | Réservé aux HDD ou fichiers ; refusé sur les supports flash à blocs remappés. |
| `purge` | Demande une sanitisation au contrôleur et suit son état. | Actuellement limitée au NVMe direct sous Linux et non qualifiée sur matériel réel. |
| `destroy` | Décrit une destruction physique externe. | Refusée par `erase` ; aucun bricking logiciel n’est fourni. |

`--verify full` relit toute la portée logique après `clear`. `--verify sample` ne contrôle que trois zones et constitue une preuve plus faible. Pour une purge native, la vérification indiquée comme `contrôleur` représente le statut rapporté par le contrôleur ; ce n’est pas une relecture logique complète ni une preuve forensique indépendante.

## Barrières d’exécution physique

Lorsqu’une future version possédera un tuple réel qualifié, `erase` exigera notamment :

- un disque entier, non monté, non utilisé, non système et non en lecture seule ;
- un état de sûreté établi sans ambiguïté ;
- l’identifiant stable exact dans `--confirm` ;
- `--acknowledge-data-loss` ;
- un journal régulier distinct de la cible dans `--audit` ;
- un opérateur et un témoin distincts ;
- `--allow-internal` pour un disque interne, depuis un environnement de maintenance indépendant ;
- le tuple exact qualifié, sauf dans un binaire `-lab` réservé à l’homologation.

Une confirmation valide ne remplace aucun de ces contrôles. Toute reconnexion ou dérive d’identité annule le résultat attendu. Avant le lancement, l’opération ne doit pas démarrer ; après `en_cours` mais avant modification, diskpurge conclut `échoué` ; si l’identité finale ne peut plus être établie, il conclut `indéterminé`.

## États d’audit

| État | Signification bornée | Suite opératoire |
| --- | --- | --- |
| `refusé` | La validation a refusé avant le lancement de la commande destructive. | Corriger la cause seulement si la procédure l’autorise ; ne jamais contourner un refus de qualification. |
| `en_cours` | L’intention d’exécuter a été journalisée et synchronisée avant l’appel destructif. | Ce seul état ne prouve ni l’acceptation par le matériel ni l’achèvement. Une opération qui reste ainsi est un incident. |
| `réussi` | Le chemin prévu par le programme et sa vérification ont terminé sans erreur, avec une identité finale observée. | Vérifier le journal et les preuves exigées par la qualification avant toute remise en service. |
| `échoué` | Une erreur déterminée a interrompu l’opération ou sa vérification. Des données peuvent déjà avoir été modifiées. | Mettre le support en quarantaine et appliquer la procédure d’incident. |
| `indéterminé` | Le résultat terminal ou l’identité finale n’a pas pu être établi de façon fiable. Une commande native peut déjà avoir été acceptée ou avoir démarré. | Quarantaine immédiate ; ne pas réutiliser, reconnecter ou relancer sans décision d’incident. |

Un refus autonome est terminal. Toute autre opération valide suit `en_cours`, puis exactement un état terminal parmi `réussi`, `échoué` et `indéterminé`.

Exemples documentaires, volontairement incomplets et non vérifiables comme journal :

~~~json
{"statut":"refusé","détail":"Tuple matériel non qualifié"}
{"statut":"en_cours","détail":"Effacement lancé"}
{"statut":"réussi","détail":"Écriture et vérification terminées"}
{"statut":"échoué","détail":"Effacement interrompu ; état partiellement effacé"}
{"statut":"indéterminé","détail":"Commande acceptée, suivi perdu"}
~~~

## Codes de sortie

| Code | Interprétation |
| ---: | --- |
| 0 | Commande terminée avec succès, ou plan exécutable selon la commande appelée. |
| 1 | Refus, erreur déterminée, échec d’effacement, d’audit ou de vérification. L’état réel doit être lu avec le message et le journal. |
| 2 | Syntaxe, option ou valeur CLI invalide ; inclut la demande de `--lab-mode` au binaire standard. |
| 3 | Plan non résolu, indisponible ou non exécutable. |
| 4 | Résultat d’effacement indéterminé, notamment après une commande native possiblement acceptée ou une identité finale non vérifiable. Quarantaine obligatoire. |

Le code 1 après l’exécution peut aussi signaler l’impossibilité d’écrire l’état terminal d’audit. Il ne permet donc pas, à lui seul, de conclure que les données n’ont pas changé.

## Interruption et reprise

Sur `clear`, SIGINT ou SIGTERM demande l’arrêt. Le programme tente de vider les tampons et de synchroniser les données déjà écrites, mais le chemin d’interruption ne confirme pas la réussite de cette synchronisation. Il signale ensuite un échec partiel lorsqu’il peut encore écrire le journal. Le support n’est alors ni intact ni valablement assaini.

Pour une purge native, arrêter le processus de suivi n’annule pas nécessairement la commande dans le contrôleur. Elle peut continuer après la perte du processus ou un redémarrage. Ne coupez pas l’alimentation, ne débranchez pas et ne relancez pas aveuglément. Stabilisez l’alimentation, isolez la cible et suivez la procédure de réponse à incident et le protocole propre au tuple qualifié.

diskpurge ne fournit pas de reprise persistante universelle. Tout `en_cours` sans terminal, tout code 4 et toute disparition de la cible impose une quarantaine.

## Clôture et conservation des preuves

Après une opération autorisée :

1. Exécuter `verify-audit` sur une copie contrôlée du journal.
2. Conserver l’empreinte finale affichée avec la version, le tuple, l’autorisation et les preuves indépendantes prévues par la qualification.
3. Signer ou ancrer cette empreinte dans un service externe de confiance et transférer le journal vers un stockage en ajout seul.
4. Ne produire aucun certificat client si le journal est incomplet, si son authenticité externe manque ou si une preuve requise échoue.
5. Ne remettre un support en service qu’après un état `réussi`, une identité finale cohérente et la validation de toutes les barrières de la publication concernée.

La chaîne SHA-256 locale détecte les ruptures non recalculées. Elle n’authentifie ni l’auteur ni l’heure et une personne capable de remplacer le fichier complet peut recalculer la chaîne. Consultez [le schéma d’audit](AUDIT_SCHEMA.md) et [la procédure de réponse à incident](INCIDENT_RESPONSE.md).
