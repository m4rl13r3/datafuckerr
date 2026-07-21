# Journal des changements

Toutes les modifications notables de diskpurge sont consignées dans ce fichier.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet entend suivre [Semantic Versioning](https://semver.org/lang/fr/) à partir de sa première préversion publiée.

## 0.2.4 — 2026-07-21

### Corrigé

- Le smoke test de l’AppImage finale impose désormais le backend Qt sans affichage et ne dépend plus de l’environnement du workflow appelant.

## 0.2.3 — 2026-07-21

### Corrigé

- Les textes de licence redistribués par les applications natives sont désormais embarqués et vérifiés depuis le dépôt afin que la construction ne dépende plus d’un téléchargement réseau.

## 0.2.2 — 2026-07-21

### Corrigé

- La construction Ubuntu 22.04 de Publication compile uniquement le moteur standard destiné à l’AppImage ; les tests Linux complets restent exécutés par les jobs CI dédiés.
- Les compilations de Publication sont limitées à deux tâches parallèles pour stabiliser les runners.

## 0.2.1 — 2026-07-21

### Ajouté

- Application autonome Linux x64 distribuée au format AppImage.
- Application autonome macOS Apple Silicon distribuée dans une image DMG.
- Application autonome Windows x64 distribuée avec un installateur Inno Setup.
- Intégration du moteur C standard, de Qt, de Python et du générateur PDF dans chaque application native.
- Smoke test exécuté sur chaque application construite avant publication.
- Somme SHA-256 et attestation de provenance GitHub pour chaque paquet natif.

### Modifié

- La publication contient désormais dix-sept fichiers, dont les trois applications autonomes et leurs sommes.
- La résolution du moteur et la génération de rapports prennent en charge une exécution PyInstaller gelée.

### Limites connues

- Les applications macOS et Windows ne sont ni signées par un certificat développeur ni notarisées ; les avertissements du système restent donc attendus.
- Les paquets autonomes ne constituent aucune qualification matérielle et conservent le refus des périphériques physiques de la version standard.

## 0.2.0 — 2026-07-21

### Ajouté

- Licence Apache License 2.0.
- Politique de sécurité et canal de divulgation coordonnée.
- Guide de contribution et code de conduite.
- Modèle de menace, protocole de qualification matérielle et checklist de publication.
- Vérification stricte des valeurs de l’option de vérification et vérification complète par défaut.
- Refus des partitions comme cibles physiques.
- Réinspection de l’identité, de la capacité et du type avant l’exécution.
- Identifiant d’opérateur, audit et acquittement explicite obligatoires pour un disque physique.
- Journal JSON Lines verrouillé, synchronisé et chaîné par SHA-256.
- Commande verify-audit détectant troncature, rupture et modification de la chaîne.
- Gestion des signaux avec état d’interruption détaillé dans l’audit lorsqu’il reste accessible.
- Publication automatisée multi-plateforme avec mode laboratoire désactivé et actions épinglées par empreinte.
- Archives déterministes, manifeste intégré, sommes SHA-256, nomenclature CycloneDX 1.5 et attestations de provenance GitHub.
- Interface graphique Qt 6 pour les plateformes maintenues, avec refus central du mode laboratoire.
- Générateur de rapports PDF techniques non certifiés depuis un journal préalablement accepté par verify-audit.
- Recette et workflow d’image Debian Live graphique amd64 pour BIOS et UEFI.

### Modifié

- README clarifié pour afficher le statut alpha et la matrice réelle de fonctionnalités.

### Fonctionnalités initiales

- Inventaire, inspection et planification de supports.
- Confirmation destructive liée à l’identifiant exposé.
- Écrasement à zéro avec synchronisation et vérification complète ou échantillonnée.
- Journal d’audit JSON Lines optionnel pour les fichiers de test.
- Garde-fous contre les supports montés détectés, le disque système détecté, la lecture seule et les types inconnus.
- Backend expérimental NVMe Sanitize direct sous Linux, avec choix entre crypto-erase et block-erase et suivi du journal Sanitize.
- Démonstrations accélérées sur fichiers virtuels.

### Limites connues

- Aucune qualification matérielle publiée.
- Backend NVMe destructif non validé sur une matrice de contrôleurs sacrifiables.
- Aucun backend natif ATA ou SCSI.
- Aucun backend de purge native sous macOS ou Windows.
- Détection Windows insuffisante pour autoriser un usage sur un disque réel.
- Chaîne d’audit sans signature ni ancrage externe, donc sans garantie d’authenticité.
- Effacement physique clear désactivé dans le binaire standard tant que son identité matérielle ne peut pas être reliée au descripteur destructif.
- Revalidation NVMe native non encore suffisamment liée au descripteur destructif.
- Reprise persistante après interruption encore incomplète.
