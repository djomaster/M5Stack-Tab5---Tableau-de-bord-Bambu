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

## Sauvegarde

- Pousser régulièrement le dépôt git vers un dépôt distant privé.
- Garder une copie du fichier de configuration des imprimantes hors du projet (il contient des codes d'accès).
