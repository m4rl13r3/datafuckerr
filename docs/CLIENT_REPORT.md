# Rapport PDF client non certifié

Le générateur transforme un journal JSON Lines de diskpurge en une synthèse PDF lisible. Il produit une **trace technique d’exécution**, jamais un certificat d’effacement.

Chaque page porte la mention `RAPPORT TECHNIQUE NON CERTIFIÉ`. Le document rappelle explicitement l’absence de validation matérielle, de test indépendant, de signature et d’ancrage externe de la chaîne locale.

## Prérequis

- un binaire `diskpurge` construit depuis ce dépôt ou disponible dans le `PATH` ;
- Python 3.9 ou plus récent ;
- les dépendances de [tools/report/requirements.txt](../tools/report/requirements.txt).

~~~sh
python3 -m pip install -r tools/report/requirements.txt
~~~

## Génération

Depuis la racine du dépôt :

~~~sh
python3 tools/report/generate_report.py journal.jsonl \
  --client "Client Exemple" \
  --reference "DOSSIER-2026-001"
~~~

La sortie par défaut est créée sous `output/pdf/`, avec un nom dérivé du journal. Un chemin explicite peut être fourni :

~~~sh
python3 tools/report/generate_report.py journal.jsonl \
  --diskpurge build/diskpurge \
  --output output/pdf/rapport-client.pdf
~~~

Une sortie existante n’est jamais remplacée silencieusement. L’option `--force` autorise son remplacement atomique après validation complète du nouveau rapport.

## Ordre des contrôles

Le générateur :

1. exécute `diskpurge verify-audit <journal>` avec une liste d’arguments et sans interpréteur de commandes ;
2. refuse immédiatement le journal si le vérificateur retourne un échec ou une sortie inattendue ;
3. lit ensuite le JSONL, exige le schéma fermé, les types attendus et un état terminal pour chaque opération ;
4. compare le nombre d’enregistrements et l’empreinte finale analysés avec le résultat de `verify-audit` ;
5. écrit le PDF dans un fichier temporaire puis le publie par remplacement atomique.

Aucun PDF n’est conservé lorsqu’un journal est invalide, vide, tronqué, incomplet, modifié pendant sa lecture ou différent de la source acceptée par le vérificateur.

## Contenu et portée

Le rapport distingue :

- le résultat consigné et son détail ;
- la méthode demandée, la méthode exécutée et la vérification déclarée ;
- les identités déclarées de l’opérateur et du témoin ;
- les identités du support observées avant et après l’opération ;
- la source et la valeur brute du statut natif ;
- les empreintes du fichier et de la chaîne d’audit ;
- les limites de la preuve locale.

Même lorsqu’un statut `réussi` est présent, le verdict reste limité à une trace locale cohérente. Le document ne prouve pas l’absence de données résiduelles, l’authenticité des personnes, l’exactitude du firmware, l’heure réelle ou la bonne exécution physique de la méthode.

Pour un dossier client plus probant, conserver ensemble le JSONL original, le PDF, l’identité exacte du binaire et, lorsqu’ils seront disponibles, une signature ou un ancrage externe, une qualification matérielle applicable et les résultats d’une revue indépendante.

## Tests non destructifs

Les tests utilisent uniquement des journaux synthétiques dans un répertoire temporaire. Ils n’invoquent aucune commande d’effacement et n’accèdent à aucun périphérique.

~~~sh
python3 -m unittest discover -s tools/report/tests -v
~~~
