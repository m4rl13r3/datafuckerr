# Interface graphique datafuckerr

L’interface de bureau repose sur Qt 6 avec PySide6. Son écran principal se limite à trois étapes : rechercher un support, confirmer son identité et lancer la réinitialisation. Les réglages techniques, les chemins manuels et les journaux détaillés restent masqués par défaut. Elle pilote le binaire C standard sans réimplémenter l’effacement, sans interpréteur de commandes et sans transmettre `--lab-mode`.

Le thème transpose les conventions shadcn dans Qt : tokens sémantiques neutres, surfaces sans ombre décorative, bordures fines, variantes `default`, `outline`, `secondary`, `ghost` et `destructive`, champs composés avec leur libellé et focus visible. Le rendu conserve une seule colonne de travail et ne modifie aucune règle d’autorisation du moteur.

## Parcours et sécurité

Les commandes sont transmises à `QProcess` sous la forme d’un programme et d’une liste d’arguments. Aucun support n’est présélectionné : l’utilisateur doit le choisir explicitement dans l’inventaire non destructif. Le chemin manuel est réservé aux options avancées.

Le parcours d’effacement impose :

- une inspection suivie d’un plan déclaré exécutable par le cœur ;
- le refus d’un binaire dont la version porte le suffixe `-lab` ;
- une confirmation manuelle des six derniers caractères de l’identifiant stable ;
- un journal d’audit distinct de la cible, créé automatiquement sauf choix contraire ;
- un opérateur et un témoin différents ;
- l’acquittement explicite de la perte de données ;
- une nouvelle inspection juste avant la dernière confirmation ;
- la comparaison de l’identité et de l’état observés avec l’inspection planifiée.

Le cœur C effectue encore sa propre réinspection et reste seul responsable de l’autorisation finale. La version distribuée ne possède aucun tuple matériel réel qualifié : les supports physiques restent donc refusés. L’interface ne constitue ni une qualification matérielle, ni un certificat d’effacement.

## Installation et lancement

Les applications autonomes s’utilisent sans environnement Python séparé :

- sous Linux x64, rendre `datafuckerr-0.2.4-linux-x64.AppImage` exécutable puis l’ouvrir ;
- sous macOS Apple Silicon, ouvrir le DMG et déplacer `datafuckerr.app` vers Applications ;
- sous Windows x64, exécuter `datafuckerr-0.2.4-windows-x64-setup.exe`.

Cette alpha n’a pas encore de certificat de signature Apple ou Microsoft. Vérifiez d’abord la somme SHA-256 et l’attestation de provenance depuis la page de prépublication. Ne désactivez pas globalement les protections du système pour lancer l’application.

Le lancement depuis les sources reste disponible pour le développement. Construisez d’abord le binaire standard, puis installez la dépendance graphique :

~~~sh
make
python3 -m pip install -r ui/requirements.txt -r tools/report/requirements.txt
./ui/run_gui.sh
~~~

Sous macOS, `run_gui.command` peut aussi être ouvert depuis le Finder. Sous Windows :

~~~text
py -3 -m pip install -r ui\requirements.txt -r tools\report\requirements.txt
ui\run_gui.cmd
~~~

Le chemin du binaire peut être passé au lanceur ou choisi dans la fenêtre :

~~~sh
./ui/run_gui.sh /chemin/vers/diskpurge
~~~

Sous Linux ou macOS, la variable `PYTHON` permet de choisir explicitement le runtime :

~~~sh
PYTHON=/chemin/vers/python3 ./ui/run_gui.sh
~~~

Le poste doit fournir Python 3.10 ou supérieur. La dépendance de bureau est figée sur `PySide6-Essentials 6.11.1`. PySide6 est proposé sous LGPLv3, GPLv3 ou licence commerciale ; toute redistribution doit respecter la licence retenue et fournir les mentions et mécanismes requis.

Les prépublications fournissent aussi des paquets autonomes qui embarquent Python, Qt, le moteur standard et le générateur PDF : AppImage Linux x64, DMG macOS Apple Silicon et installateur Windows x64. Le fichier `packaging/native/THIRD_PARTY_NOTICES.md` décrit les composants redistribués. Le DMG et l’installateur de cette alpha ne possèdent pas encore de signature développeur reconnue par Apple ou Microsoft.

## Accessibilité

- composants Qt Widgets standards exposés aux technologies d’assistance du système ;
- parcours complet au clavier et focus visible ;
- `Alt+1` pour la réinitialisation et `Alt+2` pour les rapports ;
- `Ctrl++`, `Ctrl+-` et `Ctrl+0` pour l’échelle des textes ;
- `Ctrl+Maj+C` pour le contraste renforcé ;
- libellés associés aux champs et noms accessibles explicites pour les actions critiques ;
- état destructif signalé par du texte et non par la couleur seule.

Une validation manuelle avec NVDA, Narrator, VoiceOver et Orca reste nécessaire avant une annonce de conformité formelle.

## Compatibilité annoncée

| Système | Portée de l’interface |
| --- | --- |
| Linux x64 maintenu | AppImage autonome ou lancement depuis les sources avec Python 3.10+. |
| macOS 13 à 26 sur Apple Silicon | Application autonome dans un DMG ou lancement depuis les sources. |
| macOS 12 et antérieurs | Non pris en charge par la dépendance graphique distribuée. |
| Windows 10 et 11 x64 | Installateur autonome ou lancement depuis les sources avec Python 3.10+. |
| Windows XP, 7 et 8 | Non pris en charge ; utiliser le média amorçable sur un matériel compatible. |

Promettre Windows XP, 7 ou 8 pour un outil destructif serait trompeur : ces systèmes ne constituent plus une base maintenue et la pile Qt/Python actuelle ne les cible pas. L’ISO Linux est la voie prévue pour les postes qui ne peuvent pas exécuter une plateforme moderne, sans contourner pour autant la qualification matérielle.

## Tests

Depuis la racine du dépôt :

~~~sh
python3 -m unittest ui.test_diskpurge_commands -v
QT_QPA_PLATFORM=offscreen python3 -m unittest ui.test_datafuckerr_qt -v
~~~

La première suite vérifie les constructions de commandes et les garde-fous indépendants de l’interface. La seconde instancie réellement Qt hors écran, contrôle la navigation, les propriétés accessibles, le thème et l’échec propre d’un lancement de processus. Aucun test ne vise un périphérique réel.

Les quatre captures de référence sont générées avec des valeurs entièrement simulées et avec tout lancement de commande neutralisé :

~~~sh
QT_QPA_PLATFORM=offscreen python3 tools/ui/capture_interface.py
~~~

Le script rend successivement l’état initial, la confirmation, les rapports et le dialogue irréversible. Les fenêtres principales utilisent 1920 × 1080 par défaut. Il ne requiert aucun support et ne lance ni inspection ni effacement.
