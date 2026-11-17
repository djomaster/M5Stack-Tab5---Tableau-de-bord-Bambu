# Procédures

## Compiler et téléverser

1. Ouvrir le dossier du projet dans VS Code (extension PlatformIO).
2. Aucun fichier de configuration : le WiFi se règle sur l'appareil (Réglages, WiFi).
3. Compiler l'environnement `tab5`. Le premier build télécharge la plateforme pioarduino : prévoir plusieurs minutes.
4. Brancher le Tab5 avec un câble USB de données, puis téléverser.
5. Ouvrir le moniteur série (115200) pour lire les messages.

## Si l'installation de la plateforme échoue

- Mettre à jour PlatformIO (VS Code : extension PlatformIO, ou `python -m pip install -U platformio`).
- Activer les chemins longs de Windows, puis redémarrer.
- Vérifier que Python et git sont installés et accessibles depuis un terminal.
- Relancer, et en cas d'échec noter le message complet dans `docs/TESTS.md`.

## Si le Tab5 n'est pas reconnu

- Utiliser un vrai câble de données, fermer tout autre moniteur série.
- Mettre le Tab5 en mode téléchargement (voir la documentation M5Stack du Tab5), puis relancer le téléversement.

## Activer le mode LAN sur une imprimante Bambu Lab

À faire sur l'écran de chaque imprimante (menu réseau) : activer le mode LAN, noter l'adresse IP, le code d'accès et le numéro de série (dans les infos de l'appareil). Pour la caméra de la X1C, activer aussi l'affichage en direct en mode LAN. Les intitulés exacts varient avec la version du firmware : se référer à la documentation Bambu Lab de la version installée.

## Compilation automatique sur GitHub (GitHub Actions)

Le fichier `.github/workflows/build.yml` compile le firmware sur GitHub, sans rien installer.

- **À chaque envoi sur `main`** : onglet **Actions** du dépôt, une coche verte signifie que le projet compile.
- **Pour joindre le firmware à une version** : publier une **Release** (Releases > Draft a new release, choisir le tag, Publish). Au bout de quelques minutes (la première compilation est longue), les fichiers `bambu-dashboard-<tag>-complet.bin`, `-app.bin`, `-bootloader.bin` et `-partitions.bin` apparaissent dans la release.
- **Pour une release déjà publiée** : Actions > « Compilation du firmware » > Run workflow, saisir le tag (par exemple `v0.5.1`) dans le champ « Tag d'une release déjà publiée », Run. Les fichiers compilés sont joints à cette release (ceux du même nom sont remplacés).
- **À la demande** : Actions > « Compilation du firmware » > Run workflow ; les fichiers se téléchargent en bas de la page de l'exécution (30 jours).
- **Flasher le fichier `-complet.bin`** sans PlatformIO : un seul fichier, à écrire à l'adresse **0x0** (par exemple avec l'outil en ligne esptool-js d'Espressif, Tab5 branché en USB). Non testé à ce jour.
- Si l'exécution est rouge : ouvrir l'exécution, copier les dernières lignes de l'étape en échec et les transmettre.

## Sauvegarde

- Pousser régulièrement le dépôt git vers un dépôt distant privé.
- Garder une copie du fichier de configuration des imprimantes hors du projet (il contient des codes d'accès).
