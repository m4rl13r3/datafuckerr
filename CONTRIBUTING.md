# Contribuer à diskpurge

Merci de contribuer. diskpurge commande des opérations irréversibles : la priorité est une défaillance sûre, explicite et traçable, avant la compatibilité ou la vitesse.

En soumettant une contribution destinée au projet, vous acceptez qu’elle soit distribuée sous l’Apache License 2.0 conformément à la section 5 de la licence. Aucun accord de contribution supplémentaire n’est exigé à ce stade.

## Avant de commencer

- Consultez le [modèle de menace](docs/THREAT_MODEL.md).
- Pour un problème de sécurité non corrigé, utilisez exclusivement la procédure privée de [SECURITY.md](SECURITY.md).
- Pour une évolution destructive, ouvrez d’abord une proposition publique ne contenant aucun détail de vulnérabilité exploitable.
- Réservez les tests destructifs à des supports sacrifiables, isolés et explicitement autorisés.

## Construire et tester

Le code vise C17. Les commandes de référence sont :

~~~sh
make clean
make
make test
~~~

La construction CMake doit également rester fonctionnelle :

~~~sh
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake
~~~

Avant une demande de fusion, exécutez les tests avec les avertissements stricts, puis les sanitizers disponibles sur votre plateforme. Aucun test automatique ne doit sélectionner un périphérique bloc réel.

## Règles de conception

- Refuser l’opération lorsque l’identité, la portée, le type de support ou l’état de montage est ambigu.
- Séparer inventaire, planification, confirmation et exécution.
- Exiger une confirmation liée à une identité stable immédiatement revalidée avant la première écriture.
- Ne jamais transformer automatiquement une méthode indisponible en méthode moins forte.
- Ne pas présenter TRIM, UNMAP, un formatage de système de fichiers ou un disque « brické » comme une preuve de sanitisation.
- Traiter toute perte de suivi après acceptation d’une commande native comme un état indéterminé, jamais comme un succès.
- Écrire en anglais tous les identifiants, noms de fichiers techniques, fonctions, types, constantes et variables. Le français est réservé aux textes visibles et au schéma d’audit déjà publié.
- Conserver des messages français lisibles avec leurs accents.
- Ne pas ajouter de commentaires dans le code ; rendre les noms, les types et la structure suffisamment explicites.
- Préférer des fonctions ciblées et des abstractions justifiées par un besoin réel ; retirer les doublons, couches génériques et gabarits sans usage concret.
- Éviter les dépendances nouvelles dans le chemin privilégié et documenter toute dépendance de construction.

## Changements sensibles

Une contribution ATA, SCSI, NVMe, Windows, macOS ou liée à l’audit doit inclure :

- la référence normative exacte et la révision utilisée ;
- les hypothèses de portée de la commande ;
- des tests d’erreur et de réponses malformées sans matériel réel ;
- des vecteurs capturés ou synthétiques expurgés ;
- un comportement défini pour interruption, perte d’alimentation et reconnexion ;
- une mise à jour du modèle de menace ;
- un protocole de qualification sur matériel sacrifiable.

Les essais matériels ne suffisent pas à eux seuls. Le changement doit être testable sans accès privilégié et sans périphérique réel, par abstraction, simulation ou injection de réponses.

## Demande de fusion

Une demande de fusion doit rester ciblée et expliquer :

- le problème et le résultat attendu ;
- les risques de régression ou de mauvaise sélection ;
- les plateformes et architectures testées ;
- les commandes de validation exécutées ;
- les limites qui subsistent ;
- les documents et entrées de changelog mis à jour.

Checklist de l’auteur :

- [ ] aucun secret, journal brut, numéro de série réel ou donnée client n’est inclus ;
- [ ] aucun test ne peut effacer un périphérique réel ;
- [ ] tous les chemins d’échec refusent l’opération de façon explicite ;
- [ ] les nouvelles options rejettent les valeurs inconnues ;
- [ ] la version et la documentation restent cohérentes ;
- [ ] les tests et analyses disponibles passent ;
- [ ] le changement respecte le [Code de conduite](CODE_OF_CONDUCT.md).

Une approbation par une personne autre que l’auteur est obligatoire pour tout changement du chemin destructif. La personne qui publie une version ne doit pas être l’unique personne ayant validé ce chemin.

## Qualification et publication

Un test réussi sur un modèle ne généralise pas la prise en charge à une famille commerciale. La prise en charge n’est annoncée qu’après exécution et archivage de la [procédure de qualification matérielle](docs/HARDWARE_QUALIFICATION.md) pour le tuple exact modèle, firmware, transport, système et version de diskpurge.

Les mainteneurs utilisent la [checklist de publication](docs/RELEASE_CHECKLIST.md). Une case non vérifiée reste un blocage ; elle ne doit pas être transformée en simple avertissement pour tenir une date.
