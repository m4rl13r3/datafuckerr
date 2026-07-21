# Schéma du journal d’audit

## Statut du format

diskpurge produit un fichier JSON Lines UTF-8 : chaque enregistrement canonique occupe exactement une ligne terminée par `\n`. Le schéma courant porte le numéro `1`.

Ce format est une trace technique locale. Il ne constitue ni une signature numérique, ni un certificat d’effacement, ni une preuve de non-répudiation. `verify-audit` contrôle la représentation canonique, la chaîne SHA-256 et les règles sémantiques ci-dessous ; il ne démontre pas que le firmware a dit vrai ni que le support a été correctement assaini.

Le vérificateur est un parseur fermé du schéma diskpurge, pas un parseur JSON général. Il exige les champs, types et ordre documentés, sans espace intercalé ni clé supplémentaire ou dupliquée. Il accepte uniquement les échappements `\"` et `\\`, un UTF-8 valide et aucun caractère de contrôle ou `DEL`. Le producteur refuse ces textes avant écriture ; il ne les corrige pas silencieusement. Aucune normalisation Unicode n’est appliquée.

## Enregistrement canonique

Les champs apparaissent exactement dans cet ordre.

| Champ | Type | Sens et contrainte |
| --- | --- | --- |
| `schéma` | entier | Exactement `1`. |
| `horodatage` | chaîne | UTC `AAAA-MM-JJThh:mm:ssZ`, date civile valide à partir de 1970. L’horloge locale n’est pas une source de temps de confiance. |
| `opération` | chaîne | 64 chiffres hexadécimaux minuscules, obtenus par SHA-256 de 32 octets du générateur aléatoire cryptographique système. |
| `opérateur` | chaîne | Identité déclarée de l’opérateur ; obligatoire sur support physique. |
| `témoin` | chaîne | Identité déclarée du témoin ; obligatoire, distincte de l’opérateur sur support physique. |
| `version` | chaîne | Version du binaire ; suffixe `-lab` pour un build laboratoire. |
| `périphérique` | chaîne | Chemin présenté au programme. |
| `identifiant` | chaîne | Identifiant stable observé et confirmé avant l’opération. |
| `identifiant_après` | chaîne | Identifiant observé après l’opération, ou chaîne vide sans observation finale. |
| `taille_après` | entier non signé | Capacité finale observée, ou `0` sans observation finale. |
| `identité_après_observée` | booléen | Indique si l’identité finale a été acquise. Sa valeur doit correspondre à la présence d’`identifiant_après`. |
| `modèle` | chaîne | Modèle observé avant l’opération. |
| `firmware` | chaîne | Révision de firmware observée avant l’opération. |
| `transport` | chaîne | Transport exposé par le système. Il ne prouve pas qu’une commande traverse un pont. |
| `environnement` | chaîne | Système, version de noyau et architecture observés lorsque la plateforme les expose. |
| `topologie` | chaîne | Empreinte textuelle du chemin matériel ou virtuel observé. |
| `qualification` | chaîne | Identifiant du tuple qualifié, chaîne vide sinon. `VIRTUEL-TEST` ne qualifie qu’un fichier régulier de test. |
| `laboratoire` | booléen | Vaut `true` uniquement si le contournement du build `-lab` a été demandé. |
| `taille` | entier non signé | Capacité en octets avant l’opération. |
| `taille_bloc` | entier non signé | Taille de bloc logique observée avant l’opération. |
| `type_support` | chaîne | `HDD`, `SSD`, `NVMe`, `flash`, `fichier` ou `inconnu`. |
| `disque_entier` | booléen | Indique la portée disque entier observée. |
| `identité_unique` | booléen | Indique si un identifiant matériel crédible a été acquis ; la qualification doit encore prouver son unicité réelle. |
| `méthode_demandée` | chaîne | `auto`, `clear-zero`, `purge-native` ou `destruction-physique`. |
| `méthode_exécutée` | chaîne | Méthode résolue avant l’exécution. |
| `sous_méthode_native` | chaîne | `aucune`, `nvme-block-erase` ou `nvme-crypto-erase`. |
| `source_statut_natif` | chaîne | `aucune`, `nvme-commande` ou `nvme-journal-sanitize`. Elle empêche de confondre un statut de commande et le champ SSTAT du journal Sanitize. |
| `statut_natif_observé` | booléen | Indique si `statut_natif_brut` provient réellement de la source déclarée. |
| `statut_natif_brut` | entier non signé | Valeur brute, limitée à 32 bits. `0` en l’absence d’observation. |
| `vérification` | chaîne | `complète`, `échantillonnée` ou `contrôleur`. |
| `statut` | chaîne | `refusé`, `en_cours`, `réussi`, `échoué` ou `indéterminé`. |
| `détail` | chaîne | Diagnostic technique ; aucune donnée client ne doit y être placée. |
| `précédente` | chaîne | Empreinte de l’enregistrement précédent, ou 64 zéros pour le premier. |
| `empreinte` | chaîne | Empreinte SHA-256 calculée comme ci-dessous. |

Une ligne, saut final compris, doit faire moins de 16 384 octets. Un journal contient au plus 8 192 enregistrements et 4 096 opérations. Il faut ouvrir un nouveau journal avant d’atteindre ces limites, puis ancrer séparément l’empreinte finale de l’ancien. Le fichier doit être régulier, sans lien symbolique, point de réanalyse ou lien physique supplémentaire. L’écriture et la vérification refusent un verrou concurrent.

## Construction de la chaîne

Le producteur construit d’abord le JSON compact allant de `schéma` à `détail`, accolade fermante comprise. Appelons ces octets `charge`. Pour la première ligne, `précédente` vaut 64 zéros ; pour les suivantes, elle reprend l’`empreinte` immédiatement antérieure.

~~~text
empreinte = hex_minuscule(SHA-256(octets(précédente) || charge))
~~~

Le producteur retire ensuite l’accolade finale de `charge`, ajoute `précédente` et `empreinte`, referme l’objet et écrit `\n`. Ces deux champs et le saut final ne font donc pas partie de `charge`. Toute variation d’ordre, d’espace, d’échappement ou d’octets change l’empreinte et est refusée par le parseur canonique.

## Automate et cohérence

Chaque identifiant d’opération suit exactement l’un des parcours :

~~~text
nouvelle opération ── refusé
nouvelle opération ── en_cours ── réussi
                              ├── échoué
                              └── indéterminé
~~~

- `refusé` est autonome et terminal ;
- `en_cours` apparaît exactement une fois avant un terminal exécuté ;
- un second démarrage, un second terminal, un terminal orphelin ou un identifiant réutilisé invalide le journal ;
- une opération restée `en_cours` invalide la vérification et bloque l’ajout d’une nouvelle opération ;
- `réussi` exige une identité finale observée ; sans observation, `identifiant_après` est vide et `taille_après` vaut `0` ;
- `refusé` et `en_cours` ne portent ni identité finale ni statut natif ;
- une méthode non native ne peut porter aucun statut natif ;
- une purge native réussie exige `source_statut_natif = nvme-journal-sanitize` et un SSTAT terminal `1` ou `4` après que l’exécuteur a observé l’état en cours ;
- une perte de suivi, une acceptation incertaine ou l’impossibilité de persister l’état terminal donne le code de sortie `4` et le verdict `indéterminé` lorsqu’il peut encore être écrit.

## Contexte immuable

Entre le démarrage et le terminal d’une même opération, `verify-audit` exige l’identité exacte de :

- `schéma`, `laboratoire`, `taille`, `taille_bloc`, `disque_entier` et `identité_unique` ;
- `opérateur`, `témoin`, `version`, `périphérique`, `identifiant`, `modèle`, `firmware`, `transport`, `environnement`, `topologie` et `qualification` ;
- `type_support`, `méthode_demandée`, `méthode_exécutée`, `sous_méthode_native` et `vérification`.

L’horodatage, le détail, le statut, l’identité finale et la preuve native observée varient avec la transition.

## Ce que vérifie `verify-audit`

Sur un fichier non vide, la commande contrôle :

- la représentation canonique exacte, l’UTF-8, les types, valeurs fermées, dates et limites numériques ;
- la présence du saut final et l’absence de suffixe non authentifié ;
- le raccordement depuis 64 zéros et le recalcul SHA-256 de chaque charge ;
- l’unicité des identifiants d’opération, l’automate et l’absence d’opération orpheline ;
- l’immuabilité du contexte, la cohérence de l’identité finale et celle des statuts natifs ;
- les limites de taille, de nombre d’enregistrements et d’opérations.

Un succès signifie uniquement que le fichier est cohérent avec ces règles. Il ne prouve pas l’heure réelle, l’identité des personnes, l’identité physique du support, la portée effective du firmware, l’absence de données résiduelles ou l’authenticité de l’auteur.

## Authenticité et conservation

La chaîne détecte une altération qui n’est pas suivie d’un recalcul cohérent. Elle n’emploie aucun secret : une personne capable de remplacer tout le fichier peut fabriquer une nouvelle chaîne valide.

Avant tout usage client, il faut au minimum :

1. transférer le journal hors de la station ;
2. signer ou ancrer l’empreinte finale auprès d’un service externe de confiance avec un horodatage fiable ;
3. conserver ensemble le journal, l’ancrage, l’identité du binaire et le dossier de qualification applicable dans un stockage append-only ;
4. appliquer une politique de contrôle d’accès et de rétention validée.

Sans cette couche, le journal reste une trace locale chaînée et non un certificat client.

## Évolution

Tout changement de champ, type, ordre, sémantique ou algorithme exige un nouveau numéro de schéma et une stratégie explicite de compatibilité. Une évolution ne doit jamais requalifier silencieusement `échoué` ou `indéterminé` en `réussi`.
