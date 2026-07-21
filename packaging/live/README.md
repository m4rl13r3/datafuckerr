# ISO graphique datafuckerr Live

Cette recette construit une image Debian Live amd64 avec l’interface Qt 6 et le binaire C standard. Elle vise un démarrage BIOS ou UEFI depuis une clé USB ou un média optique.

L’image ne contient jamais le mode laboratoire. La table de qualification physique de la version 0.2.3 est vide : même démarré depuis l’ISO, datafuckerr refuse donc tout effacement de support physique. Les fichiers virtuels restent utilisables pour les démonstrations non destructives.

## Construction

Sur Debian 13 ou un système Linux compatible :

~~~sh
sudo apt-get update
sudo apt-get install -y build-essential live-build
sudo packaging/live/build.sh
~~~

L’image et son empreinte SHA-256 sont écrites dans `output/iso/`. Sur macOS ou Windows, utilisez le workflow GitHub `ISO graphique` ou une machine virtuelle Linux ; la recette ne prétend pas pouvoir exécuter `live-build` nativement sur ces systèmes.

## Démarrage et limites

- architecture : x86-64 uniquement ;
- amorçage : BIOS et UEFI ;
- Secure Boot : non revendiqué tant que l’image produite n’a pas été signée et vérifiée ;
- Apple Silicon : non pris en charge par cette image amd64 ;
- Mac Intel : démarrage externe possible selon le firmware et la politique de sécurité du modèle, sans qualification annoncée ;
- pilotes et ponts de stockage : leur présence dans Linux ne constitue pas une qualification d’effacement ;
- validation : la recette est automatisée, mais l’image doit encore subir un test de démarrage réel BIOS/UEFI et une revue indépendante avant diffusion opérationnelle.

La construction dépend des archives Debian au moment de son exécution. Pour obtenir une reproductibilité binaire stricte, il faut figer un instantané de paquets, la version de `live-build` et `SOURCE_DATE_EPOCH`, puis publier ces paramètres avec l’ISO.
