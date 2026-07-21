# Questions fréquentes

## diskpurge est-il prêt pour l’effacement de disques clients ?

Non. Le projet est une alpha expérimentale. Le code peut être publié et étudié comme tel, mais il ne doit pas être présenté comme un produit d’effacement certifié ou utilisé sur des données client dans son état actuel.

Le binaire standard ne contient aujourd’hui aucun tuple réel de qualification matérielle. Il refuse donc l’effacement de tout disque physique, quel que soit le système d’exploitation. Les fichiers réguliers portent la qualification `VIRTUEL-TEST`, réservée aux démonstrations et aux tests logiciels.

## Pourquoi le binaire standard refuse-t-il un disque physique ?

Une méthode n’est acceptable que pour un tuple exact comprenant notamment la plateforme, le modèle, le firmware, le transport, l’environnement, la topologie, le type de support, la méthode résolue, la sous-méthode native, la capacité, la taille de bloc logique et le niveau de vérification. La table distribuée ne contient aucun tuple physique de production. Ce refus est un garde-fou intentionnel, pas un problème de détection à contourner.

## À quoi sert le build de laboratoire ?

Il sert à qualifier du matériel sacrifiable sur un banc isolé. Il autorise `--lab-mode`, affiche une version suffixée `-lab` et inscrit cette version ainsi que `laboratoire: true` dans l’audit.

Ce build n’est pas destiné à un poste client, à un support de production ou à une prestation d’effacement. Le binaire standard rejette `--lab-mode`.

## USB, USB-C, SATA et PCIe sont-ils tous pris en charge de la même façon ?

Non. USB-C décrit un connecteur, pas le protocole effectivement transporté. Derrière le même connecteur peuvent se trouver de l’USB, du PCIe, du Thunderbolt ou un pont qui traduit ou filtre les commandes.

Un pont USB vers SATA ou NVMe peut masquer l’identité, les capacités et les statuts du support, ou empêcher une commande native de sanitisation. Le fait que le système puisse lire et écrire un disque ne prouve pas que `purge-native` soit disponible ni que son résultat soit observable. Le tuple de qualification doit couvrir le pont, son firmware et le chemin réel ; changer de boîtier ou de câble peut invalider la qualification.

## Le C permet-il de contourner ces limites ?

Non. Le C donne accès aux interfaces bas niveau exposées par le système, mais il ne supprime ni les restrictions du pilote, ni les filtres d’un pont, ni les limites du firmware. La sûreté dépend davantage de l’identité fiable de la cible, de la méthode réellement transmise et de sa qualification que du langage seul.

## Peut-on garantir que les données ne seront jamais récupérables ?

Pas dans l’absolu. Une affirmation sérieuse doit préciser le type de support, la méthode, les zones couvertes, les hypothèses faites sur le firmware, la vérification réalisée et le niveau de menace. diskpurge ne promet pas l’impossibilité universelle de récupération.

Un succès logiciel ne couvre pas automatiquement les blocs remappés, une mémoire cache non volatile, une puce défaillante, un firmware trompeur ou une donnée présente sur un autre support.

## Pourquoi ne pas simplement écrire des zéros partout ?

Pour un HDD qualifié, `clear-zero` écrit toute la plage logique et peut la relire. Cela ne garantit pas toutes les zones physiques cachées dans tous les scénarios.

Pour un SSD, un NVMe ou une mémoire flash, le contrôleur remappe les cellules. Une écriture logique ne permet donc pas de conclure que tous les anciens blocs physiques ont été remplacés. diskpurge refuse `clear-zero` sur ces supports.

## TRIM suffit-il à effacer un SSD ?

Non. TRIM ou la désallocation indique au contrôleur que certains blocs logiques ne sont plus utiles. Ce n’est pas une preuve que les cellules ont été effacées immédiatement ni que toutes les copies physiques sont devenues irrécupérables.

## Une purge cryptographique est-elle toujours la meilleure méthode ?

Elle peut être rapide et adaptée lorsque les données ont toujours été protégées par une clé correctement générée, stockée et détruite par un contrôleur digne de confiance. Ces hypothèses doivent être qualifiées pour le modèle et le firmware exacts. Lire des zéros après un crypto-erase ne prouve pas à lui seul que la bonne clé a été détruite.

## Combien de temps faut-il pour effacer 10 To ?

Pour un écrasement logique, la limite basse est imposée par le débit soutenu du support. Une passe sur 10 To décimaux prend théoriquement environ 13 h 53 à 200 Mo/s, ou 2 h 47 à 1 Go/s. Une écriture suivie d’une vérification complète relit la même capacité : hors ralentissements, il faut donc approximativement doubler ces durées.

Le débit réel dépend du support, du boîtier, du bus, de la température, des erreurs et de la synchronisation. Une purge native peut prendre un temps très différent et reste pilotée par le contrôleur. Il ne faut pas remplacer une vérification requise par `sample` uniquement pour gagner du temps : l’échantillonnage ne contrôle qu’une petite partie de la plage.

## Peut-on accélérer l’effacement en lançant plusieurs écritures en parallèle ?

Généralement pas sur un disque unique : le support ou le lien est déjà la ressource limitante, et des flux concurrents peuvent réduire le débit. Les gains sûrs viennent surtout d’un chemin direct correctement qualifié, d’un matériel non bridé et, lorsque le support le permet, d’une purge native qualifiée. Traiter plusieurs disques sur des contrôleurs indépendants augmente le débit global de l’atelier, pas la vitesse physique d’un disque donné.

## Le programme peut-il rendre volontairement un disque inutilisable ou le « bricker » ?

Non, et ce n’est pas un objectif acceptable du logiciel. Corrompre un firmware, envoyer des commandes hors spécification ou provoquer une panne est imprévisible, peut endommager le mauvais équipement et ne prouve pas que les données sont détruites.

La méthode `destroy` est toujours refusée par l’exécution logicielle. Lorsqu’une politique impose la destruction, elle doit être physique, réalisée sur une station et avec une procédure qualifiées, puis constatée séparément. Un disque qui ne démarre plus peut encore contenir des puces lisibles en laboratoire.

## Que signifie le statut `indéterminé` ?

diskpurge ne dispose pas d’une observation suffisante pour conclure à un résultat fiable. C’est notamment possible si une commande a pu être acceptée, si le suivi d’une commande NVMe Sanitize est interrompu, si le journal du contrôleur devient inaccessible, si l’état courant ne peut pas être distingué d’un ancien état ou si l’identité finale ne peut pas être confirmée après toute méthode.

La CLI renvoie alors le code de sortie `4` et tente d’inscrire `indéterminé` dans l’audit. Une erreur d’écriture peut empêcher cet état terminal d’y apparaître. Le support ne doit être ni réutilisé, ni rendu au client, ni déclaré effacé. Il doit être isolé et traité selon la procédure d’incident. Débrancher ou relancer immédiatement n’est pas une réponse universelle : une commande native acceptée peut continuer dans le contrôleur.

## Quelle différence y a-t-il entre `échoué` et `indéterminé` ?

`échoué` signifie que diskpurge a observé un échec ou n’a pas achevé l’opération. Pour un écrasement interrompu, le support peut être partiellement effacé. `indéterminé` signifie que l’effet final ne peut pas être attribué de manière fiable, notamment après l’acceptation possible d’une commande native. Dans les deux cas, aucune attestation positive et aucune remise en service ne sont permises.

## Un journal valide prouve-t-il l’effacement ?

Non. `verify-audit` vérifie la chaîne SHA-256, les transitions de statut, un contexte immuable et certaines règles d’identité finale. Il ne vérifie ni la sincérité des identités déclarées, ni l’horloge, ni la réalité physique de l’effacement.

La chaîne n’est pas signée. Une personne capable de remplacer tout le fichier peut recalculer une nouvelle chaîne. Un futur usage client exige une signature ou un ancrage externe de l’empreinte finale et une conservation séparée. Le format précis est décrit dans [le schéma d’audit](AUDIT_SCHEMA.md).

## Pourquoi faut-il un opérateur et un témoin ?

Pour un disque physique, diskpurge exige deux identités déclarées et distinctes. Ce double contrôle réduit le risque de cible erronée et facilite l’attribution de l’opération. Le logiciel ne vérifie toutefois pas à lui seul l’identité civile, la présence physique ou l’autorisation de ces personnes ; l’organisation doit fournir ce contrôle.

## La vérification `full` garantit-elle tout le support ?

Pour `clear-zero`, elle relit la plage logique complète écrite par le programme. Elle ne voit pas nécessairement les zones remappées ou cachées par le contrôleur. Pour une purge native, la valeur d’audit `contrôleur` indique que le résultat repose sur le statut exposé par le contrôleur ; une qualification indépendante reste nécessaire.

La vérification `sample` ne lit que des positions échantillonnées et fournit une assurance nettement plus faible.

## Un statut `réussi` suffit-il pour remettre un disque en service ?

Non. Dans un futur processus client, il faudra aussi une version publiée et vérifiée, un tuple matériel encore valide, un audit complet et authentifié, l’identité finale concordante et l’application de la politique de l’organisation. Dans l’alpha actuelle, aucun disque physique ne remplit la condition de qualification de production.

## Quelles fonctions existent selon le système ?

L’inventaire et l’inspection restent expérimentaux sur Linux, macOS et Windows. L’écrasement à zéro est expérimental sur Linux et macOS et non qualifié sur Windows. La purge native directe est actuellement limitée au NVMe sous Linux ; les purges ATA et SCSI ne sont pas implémentées. Ces capacités techniques ne changent pas le refus du binaire standard : aucun tuple matériel physique de production n’est distribué.

## Comment signaler une vulnérabilité ?

Ne publiez pas les détails dans une issue publique. Utilisez le canal privé défini dans la [politique de sécurité](../SECURITY.md), en indiquant la version, la plateforme, le scénario reproductible et l’impact supposé sans joindre de donnée client.
