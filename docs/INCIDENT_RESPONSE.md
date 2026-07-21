# Réponse aux incidents

## Portée

Cette procédure s’applique à toute opération diskpurge qui ne se termine pas par un résultat entièrement vérifié et conforme au périmètre autorisé. Elle couvre notamment une panne, une coupure d’alimentation, une perte de suivi, un disque disparu, un mauvais support suspecté et un journal incomplet ou altéré.

La version 0.2.2 standard n’est pas autorisée sur les disques clients et ne contient aucun tuple matériel réel qualifié. Un incident sur support réel pendant une expérimentation reste néanmoins traité comme critique. Le mode `-lab` ne réduit ni l’obligation de confinement ni celle de conserver les preuves.

## Réflexes immédiats

1. Suspendre les nouvelles opérations associées au même binaire, poste ou tuple.
2. Marquer physiquement la cible et la placer sous contrôle de deux personnes.
3. Ne pas la monter, la remettre en service, la remettre au client, la reconnecter à un autre poste ou relancer `erase`.
4. Ne pas couper l’alimentation ni débrancher automatiquement un support ayant reçu une commande native. Si aucune urgence électrique ou humaine ne l’impose, maintenir une alimentation stable pendant l’évaluation : le contrôleur peut poursuivre la sanitisation sans le processus diskpurge.
5. Préserver le journal original, sa copie, l’empreinte finale disponible, la sortie standard, la sortie d’erreur, la version du binaire et les journaux du système. Ne jamais modifier l’original pour le « réparer ».
6. Noter l’heure observée, l’opérateur, le témoin, le port, le câble, le boîtier et l’état des voyants sans inscrire de donnée client dans le dossier d’incident.
7. Alerter le responsable d’exploitation et le responsable sécurité. Un mauvais support possible, un faux succès ou une perte de données non autorisée est un incident majeur.

Une consigne de sécurité des personnes, d’incendie ou d’électricité prime toujours sur le maintien de l’alimentation.

## Qualification de l’état

| Observation | Ce qui est établi | Action minimale |
| --- | --- | --- |
| `refusé` sans `en_cours` | diskpurge n’a pas appelé le chemin destructif pour cette opération. | Conserver la trace et analyser le refus. Ne pas contourner une qualification absente. |
| `en_cours` sans terminal | L’intention a été synchronisée ; l’acceptation ou l’achèvement ne sont pas prouvés. | Quarantaine, conservation des preuves et recherche du statut par la procédure qualifiée du matériel. |
| `échoué` | Une erreur déterminée a été observée, mais des secteurs peuvent déjà être modifiés. | Considérer le support comme partiellement effacé et non réutilisable. |
| `indéterminé` ou code 4 | Le résultat terminal ou l’identité finale n’est pas fiable ; une commande native peut déjà avoir été acceptée ou avoir démarré. | Incident critique, alimentation stable si sûre lorsqu’une commande native est possible, quarantaine et aucune nouvelle commande. |
| `réussi` mais audit incomplet, invalide ou non ancré | Le programme peut avoir terminé, mais la preuve exigée n’est pas disponible. | Ne pas émettre de certificat et garder le support en quarantaine. |
| Disque absent ou identité différente | La cible observée ne peut plus être reliée avec certitude à la cible confirmée. | Geler le port et les supports présents ; reconstruire la chaîne de possession avant toute lecture. |

`verify-audit` contrôle la chaîne et les transitions, pas l’authenticité de l’auteur. Une vérification locale réussie ne clôt donc pas seule un incident.

## Scénarios

### Interruption d’un écrasement `clear`

Envoyer une seule interruption normale au processus si l’opération doit être arrêtée. Attendre sa sortie afin qu’il tente de vider les tampons, de synchroniser ce qui a déjà été écrit et de consigner `échoué`. Le chemin d’interruption ne confirme pas la réussite de la synchronisation. Ne supposez pas que les données restantes sont intactes ou que la partie écrite est une sanitisation complète.

Si le processus est tué, si la machine tombe ou si le journal ne reçoit pas d’état terminal, classer l’opération comme incomplète même si le support semble lisible. La seule issue autorisée est une nouvelle procédure complète, décidée après analyse, ou une destruction physique qualifiée.

### Perte de suivi d’une purge native

SIGINT, SIGTERM, la fermeture du terminal ou la panne du poste peuvent arrêter l’observation sans annuler la commande du contrôleur. Un code 4 ou l’état `indéterminé` interdit toute conclusion de succès ou d’échec.

Maintenir la cible isolée et alimentée lorsque cela est sûr. Utiliser uniquement l’outil indépendant et la procédure de reprise associés au tuple qualifié pour interroger le contrôleur. En l’absence de tuple et de procédure qualifiés, ne pas improviser une commande native : conserver le support en quarantaine ou l’orienter vers une destruction physique approuvée.

### Coupure d’alimentation ou déconnexion

Noter l’instant et la cause avant toute remise sous tension. Ne rebrancher le support qu’en environnement isolé, sur décision du responsable d’incident et selon la procédure du tuple. Une reconnexion peut changer le chemin, l’identité exposée ou l’état du contrôleur ; elle ne constitue jamais une reprise automatique.

### Mauvais support suspecté

Si un `clear` est encore suivi par diskpurge, demander l’interruption normale ; chaque seconde peut modifier davantage de données. Si une purge native a été acceptée, aucune interruption du processus ne garantit son annulation. Ne pas utiliser une coupure électrique improvisée comme mécanisme d’annulation.

Isoler tous les supports et le poste, prévenir immédiatement le propriétaire des données selon la procédure juridique, préserver les autorisations et reconstruire quelle identité physique correspondait à chaque chemin. Ne tenter ni récupération ni effacement supplémentaire avant décision commune des responsables sécurité, exploitation et juridique.

### Journal incomplet, altéré ou inaccessible

Copier le journal en lecture seule si possible, calculer son empreinte par un outil indépendant et conserver l’original. Exécuter `verify-audit` sur la copie. Garder aussi toute empreinte externe déjà ancrée.

N’ajoutez jamais manuellement un terminal et ne recalculez jamais la chaîne pour faire disparaître l’erreur. L’absence d’état terminal, l’échec de synchronisation ou une divergence d’empreinte bloque le certificat. Le résultat technique du support et la validité de la preuve sont deux questions séparées ; l’une ne répare pas l’autre.

### Succès contesté ou récupération après succès

Retirer immédiatement le tuple de toute utilisation, geler les artefacts concernés et préserver le support sans nouvelle écriture. Comparer la méthode réellement exécutée, la sous-méthode native, le firmware, le transport, la portée et les preuves indépendantes avec le dossier Q3. Traiter toute donnée récupérable au-delà du niveau de menace annoncé comme un incident de qualification et de sécurité.

## Dossier de preuve

Le dossier doit contenir, dans la mesure où ils existent :

- l’identifiant interne de l’incident et celui de l’opération ;
- le journal JSON Lines original et une copie de travail ;
- les empreintes locales et l’ancre ou signature externe ;
- le binaire exact, sa version, son manifeste, sa provenance et son empreinte ;
- les sorties standard et d’erreur, le code de sortie et les journaux de l’hôte ;
- l’identité avant et après, la capacité, le modèle, le firmware, le transport, l’environnement, la topologie et le port ;
- la méthode demandée, la méthode résolue, la sous-méthode native et le mode de vérification ;
- les identités de l’opérateur et du témoin, l’autorisation et la chaîne de possession ;
- les actions effectuées après l’incident et leurs auteurs.

Minimiser les données personnelles et ne jamais inclure le contenu du support dans un ticket général. Les preuves sensibles suivent la politique de rétention et d’accès de l’organisation.

## Quarantaine

La zone de quarantaine doit être physiquement contrôlée et séparée des stocks prêts à l’emploi. Chaque support y reçoit une étiquette inviolable et une entrée de registre indiquant au minimum l’incident, l’identité observée, l’état, la date, le détenteur et toute alimentation maintenue.

La documentation de cette exigence ne prouve pas que la zone et le registre existent. Leur création, leur contrôle d’accès et un exercice de chaîne de possession restent des conditions préalables à une publication client.

## Retrait d’un tuple ou d’une version

Lorsqu’un défaut peut concerner d’autres opérations :

1. Bloquer immédiatement le tuple et la version dans les procédures internes.
2. Suspendre leur distribution et les nouvelles attestations.
3. Identifier les opérations antérieures concernées à partir des preuves, sans télémétrie contenant des données client.
4. Publier un avis de sécurité par les canaux prévus et contacter les clients concernés après validation juridique.
5. Ne rétablir le tuple qu’après correction, nouvelle qualification complète, revue indépendante et publication de nouvelles preuves.

Une modification documentaire seule ne rétablit jamais une qualification retirée.

## Critères de clôture

Un incident ne peut être clos que lorsque :

- l’emplacement et la chaîne de possession de chaque support sont établis ;
- l’état technique est déterminé par une preuve indépendante prévue par la qualification, ou le support a subi une destruction physique qualifiée ;
- aucun certificat trompeur n’a été émis, ou il a été révoqué et ses destinataires prévenus ;
- le journal et son authenticité externe ont été évalués séparément ;
- le périmètre des versions et tuples affectés est connu ;
- les actions correctives ont un responsable, une échéance et une vérification ;
- la décision de remise en service, destruction ou conservation est signée par les rôles habilités.

Des exercices sans données réelles doivent couvrir au moins un mauvais support, une purge restée `indéterminé`, une coupure contrôlée et une fausse attestation. Tant que ces exercices et les contacts d’astreinte ne sont pas testés, les cases correspondantes de la checklist de publication restent ouvertes.

Voir aussi [le manuel opérateur](OPERATOR_MANUAL.md), [le schéma d’audit](AUDIT_SCHEMA.md) et [la checklist de publication](RELEASE_CHECKLIST.md).
