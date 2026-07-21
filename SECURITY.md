# Politique de sécurité

## Versions prises en charge

| Version | Correctifs de sécurité | Usage prévu |
| --- | --- | --- |
| Branche principale, 0.2.x | Oui, au mieux des capacités du projet | Développement et laboratoire uniquement |
| Toute autre version | Non | Non prise en charge |

Aucune version de diskpurge n’est actuellement qualifiée pour traiter des données client en production. Cette mention sera retirée uniquement après validation complète de la [checklist de publication](docs/RELEASE_CHECKLIST.md) et publication d’une matrice matérielle qualifiée.

## Signaler une vulnérabilité

Ne créez pas d’issue publique pour une vulnérabilité non corrigée.

Utilisez en priorité la fonction privée « Signaler une vulnérabilité » de l’onglet Security du dépôt. Si elle n’est pas disponible, contactez en privé l’équipe de maintenance par le canal publié sur le profil de l’organisation ou du responsable du dépôt. N’envoyez aucune donnée client, image de disque, clé, secret ou information personnelle.

Le rapport doit contenir, si possible :

- la version ou le commit concerné ;
- le système, le noyau, l’architecture et les privilèges utilisés ;
- le modèle, la révision de firmware et le transport du support ;
- un scénario de reproduction minimal réalisé sur un fichier ou un matériel sacrifiable ;
- l’impact attendu, notamment une sélection erronée, un effacement incomplet ou une fausse attestation ;
- les journaux expurgés et toute proposition de correction.

L’équipe vise un accusé de réception sous trois jours ouvrés, une première qualification sous sept jours ouvrés et un suivi au moins hebdomadaire tant que le rapport reste actif. Ces délais sont des objectifs de projet, pas une garantie contractuelle.

## Périmètre prioritaire

Les problèmes suivants sont traités comme des vulnérabilités de sécurité :

- possibilité d’effacer un autre support que celui explicitement confirmé ;
- contournement de la protection du disque système, d’un volume monté ou d’un support en lecture seule ;
- confusion d’identité après déconnexion, reconnexion, changement de chemin ou remplacement du support ;
- commande native dont la portée dépasse le périphérique présenté à l’opérateur ;
- validation incorrecte des capacités, états ou réponses ATA, SCSI ou NVMe ;
- succès annoncé alors que l’opération a échoué, reste en cours ou n’a pas été vérifiée ;
- journal d’audit ambigu, injectable, falsifié ou associé au mauvais support ;
- élévation de privilèges, corruption mémoire, lecture ou écriture hors limites ;
- dépendance, construction ou archive de publication compromise.

Une demande de nouvelle fonctionnalité, une incompatibilité matérielle clairement refusée ou une limitation déjà documentée peut être traitée publiquement lorsqu’elle ne révèle pas un moyen de contourner un garde-fou.

## Recherche sûre

Les essais destructifs sont autorisés uniquement sur des fichiers temporaires ou des supports sacrifiables que la personne effectuant le test possède et est autorisée à détruire. Isolez physiquement la station, débranchez les supports hors périmètre et n’expérimentez jamais sur une infrastructure tierce ou sur des données réelles.

Une commande de sanitisation peut continuer dans le contrôleur après l’arrêt de diskpurge. Signalez clairement cet état et conservez le support en quarantaine jusqu’à obtention d’un résultat indépendant.

Le chaînage SHA-256 et verify-audit détectent les modifications qui ne recalculent pas la chaîne. Ils n’authentifient pas le fichier et ne résistent pas à une personne capable de le remplacer entièrement. Un rapport portant sur un contournement, un recalcul ou une confusion de chaîne est dans le périmètre prioritaire.

## Divulgation coordonnée

L’équipe confirme la portée, prépare un correctif et des tests de non-régression, puis convient avec la personne ayant signalé le problème d’une date de publication. Une fenêtre de 90 jours peut servir de point de départ, mais une exploitation active ou un risque immédiat peut justifier une publication accélérée.

L’avis public doit décrire les versions touchées, l’impact, les mesures de réduction du risque et la version corrigée sans exposer de données sensibles. Le crédit est attribué avec l’accord de la personne concernée.

## Limite d’assurance

La licence ne fournit aucune garantie. Une qualification par modèle, firmware, transport et système reste obligatoire pour toute prestation professionnelle. Consultez le [modèle de menace](docs/THREAT_MODEL.md) et la [procédure de qualification matérielle](docs/HARDWARE_QUALIFICATION.md).
