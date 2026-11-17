# Plan de tests et résultats

Chaque test est exécuté sur l'appareil. Résultat : Réussi, Échoué ou Non testé. Les échecs sont décrits avant d'être corrigés.

Informations de l'appareil à compléter au premier test :
- Révision d'écran (autocollant au dos) : à renseigner
- Version M5Unified / M5GFX : à renseigner
- Version firmware X1C / H2C : à renseigner

## Étape 1 - version 0.1.0

| Réf. | Test | Résultat attendu | Résultat | Date | Remarques |
|---|---|---|---|---|---|
| T1-01 | Compilation de l'environnement `tab5` | Aucune erreur | Réussi (v0.1.2) | 2026-11-16 | v0.1.0 : `FileNotFoundError` dans `postinstall` (Windows). v0.1.1 : plateforme « stable » 55.3.312 refusée, PlatformIO Core 6.2.0-post.1 jugé trop ancien (>=6.2.0 requis). Corrigé en v0.1.2 : compilation réussie (signalée par l'utilisateur) |
| T1-02 | Téléversement | Terminé sans erreur | Réussi (v0.1.2) | 2026-11-17 | |
| T1-03 | Affichage au démarrage | Titre et version visibles | Réussi (v0.1.2) | 2026-11-17 | Version 0.1.2 affichée |
| T1-04 | Tactile | Les coordonnées changent quand on touche | Réussi (v0.1.2) | 2026-11-17 | |
| T1-05 | Batterie | Niveau cohérent | Réussi (v0.1.2) | 2026-11-17 | 67 % affiché |
| T1-06 | PSRAM | Environ 30 Mo libres affichés | Réussi (v0.1.2) | 2026-11-17 | 30 Mo affichés |
| T1-07 | WiFi | Adresse IP affichée sous 30 s | Réussi (v0.1.2) | 2026-11-17 | Adresse IP affichée : 192.168.4.186. Les broches SDIO vers l'ESP32-C6 sont validées |

## Étape 2 - version 0.2.0

À exécuter par l'utilisateur sur le Tab5. Statuts : À faire / Réussi / Échec.

| N° | Test | Résultat attendu | Statut | Notes |
|---|---|---|---|---|
| T2-01 | Compilation de l'environnement `tab5` (LVGL, polices) | « SUCCESS » sans erreur | Réussi | |
| T2-02 | Téléversement et démarrage | L'écran s'allume, pas de redémarrage en boucle | Réussi | |
| T2-03 | Orientation | Affichage en paysage 1280 × 720, pas de déformation | Réussi | |
| T2-04 | Alignement du toucher | Un appui touche le bouton visé | Réussi | |
| T2-05 | Accents | « Réglages », « À propos », « Filament épuisé » s'affichent correctement | Réussi | |
| T2-06 | Navigation | Accueil, Réglages, WiFi, À propos et retour fonctionnent | Réussi | |
| T2-07 | Recherche WiFi | La liste des réseaux apparaît en quelques secondes | Réussi | |
| T2-08 | Saisie et connexion | Mot de passe saisi (Maj, Afficher), connexion, adresse IP affichée | Réussi | |
| T2-09 | Persistance | Après redémarrage, reconnexion automatique, écran d'accueil | Réussi | |
| T2-10 | Fluidité | Pas de saccade visible au défilement et à la saisie | Réussi | |

## Étape 3 - version 0.3.0

À exécuter par l'utilisateur sur le Tab5, avec au moins une imprimante allumée en mode LAN. Notez le modèle et la version du firmware de l'imprimante dans la colonne Notes.

| N° | Test | Résultat attendu | Statut | Notes |
|---|---|---|---|---|
| T3-01 | Compilation (nouvelles bibliothèques) | « SUCCESS » | Réussi |  |
| T3-02 | Démarrage sans imprimante | Accueil avec une tuile « Ajouter une imprimante », pas de plantage | Réussi |  |
| T3-03 | Recherche automatique (SSDP) | L'imprimante apparaît dans la liste de l'étape 1 | Réussi | Recherche automatique OK (H2C) |
| T3-04 | Saisie manuelle | IP, série (Verr. Maj) et code saisis sans erreur | Réussi | |
| T3-05 | Mauvais code d'accès | Message « Code d'accès refusé » | Réussi | |
| T3-06 | Bon code d'accès | « Connexion réussie » et bouton Suivant actif | Réussi | H2C ; nécessitait le tampon MQTT de 65535 octets |
| T3-07 | Nom et enregistrement | L'imprimante apparaît dans Réglages > Imprimantes | Réussi |  |
| T3-08 | Tuile en direct | État, pourcentage, couche, températures corrects par rapport à l'imprimante | Réussi | H2C |
| T3-09 | Persistance | Après redémarrage, l'imprimante se reconnecte seule | Réussi | Alimentation débranchée puis rebranchée |
| T3-10 | Suppression | Confirmation puis disparition de la liste et de l'accueil | Réussi |  |
| T3-11 | Plusieurs imprimantes | Deux ou trois tuiles en direct, pas de plantage ni de ralentissement | À faire | Pas encore testé |
| T3-12 | Fluidité de l'écran pendant la connexion | Pas de gel de l'interface | Réussi | Fluide |
