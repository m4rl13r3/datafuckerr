# Démonstrations CLI

Les GIF sont générés à partir de véritables exécutions de `diskpurge` sur des fichiers temporaires servant de disques virtuels.

- `01-parcours-securise.gif` : inspection, plan automatique, confirmation, effacement, vérification et audit ;
- `02-garde-fous.gif` : mauvaise confirmation, purge incompatible, destruction physique et faute de méthode ;
- `03-test-virtuel-accelere.gif` : écriture et vérification complète d’un support virtuel de 128 Mio.

Le troisième GIF mesure un fichier en cache. Il valide le chemin logiciel, pas le débit d’un disque physique.

Régénération :

```sh
tools/demos/generate.sh
```
