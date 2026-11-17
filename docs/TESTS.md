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
| T3-11 | Plusieurs imprimantes | Deux ou trois tuiles en direct, pas de plantage ni de ralentissement | Réussi | H2C + X1C |
| T3-12 | Fluidité de l'écran pendant la connexion | Pas de gel de l'interface | Réussi | Fluide |

## Étape 4 - version 0.4.0

À exécuter par l'utilisateur sur le Tab5.

| N° | Test | Résultat attendu | Statut | Notes |
|---|---|---|---|---|
| T4-01 | Compilation | « SUCCESS » | Réussi | |
| T4-02 | Orientation pivotée de 180° | Interface à l'endroit, toucher bien aligné | Réussi | |
| T4-03 | Vue détaillée | Toucher d'une tuile ouvre le détail ; retour à l'accueil | Réussi | |
| T4-04 | Valeurs du détail | Progression, fichier, couche, temps, températures cohérents avec l'imprimante | Réussi | |
| T4-05 | Bobines AMS (H2C) | Couleurs, matières et pourcentages cohérents, bobine active en surbrillance | Réussi | H2C |
| T4-06 | Bobines AMS (X1C) | Idem | Réussi | Bobine externe détectée ; AMS HT de la H2C à une bobine |
| T4-07 | Sons d'essai | Les deux boutons de test émettent un son distinct | Réussi | |
| T4-08 | Volume et activation | Le volume change ; désactivé = aucun son ; réglage conservé après redémarrage | Réussi | Désactivation = silence complet |
| T4-09 | Son de fin d'impression | Mélodie quand une impression se termine | À faire | Non testé à la livraison de la 0.4.0 : nécessite une vraie fin d'impression / une vraie erreur |
| T4-10 | Son d'erreur | Alarme quand une erreur ou un échec apparaît | À faire | Non testé à la livraison de la 0.4.0 : nécessite une vraie fin d'impression / une vraie erreur |
| T4-11 | Pas de fausse alerte | Aucun son au démarrage du Tab5 ni à la reconnexion | Réussi | |

## Étape 5 - version 0.5.0

À exécuter par l'utilisateur sur le Tab5, avec un compte TigerTag (e-mail + mot de passe) qui contient des bobines.

| N° | Test | Résultat attendu | Statut | Notes |
|---|---|---|---|---|
| T5-01 | Compilation | « SUCCESS » | OK | |
| T5-02 | Barre de navigation | Trois onglets : Accueil, Inventaire, Réglages ; chacun ouvre le bon écran | OK | |
| T5-03 | Clavier : symboles | « Symb. » donne !@#$ et autres ; retour avec un second appui | OK | |
| T5-04 | Mauvais mot de passe | Message « E-mail ou mot de passe incorrect » | OK | |
| T5-05 | Connexion | « Connecté » et nombre de bobines affiché | OK | Noter l'éventuel message d'erreur (série : lignes « Tiger ») |
| T5-06 | Inventaire | Les bobines s'affichent avec couleur, marque, matière, poids | OK | Comparer avec Tiger Studio |
| T5-07 | Noms de marques et matières | Noms lisibles, pas « Marque 123 » | OK | |
| T5-08 | Filtre et total | Les pastilles de matière filtrent la liste et le total | OK | Pastilles remplacées par 3 filtres Marque/Matière/Couleur |
| T5-09 | Bobine presque vide | Poids en orange sous 100 g | OK | |
| T5-10 | Persistance | Après redémarrage, l'inventaire revient sans ressaisir le mot de passe | OK | Cache flash : « Tiger : cache charge, 74 bobines » au démarrage (log) |
| T5-11 | Lien AMS : puce RFID | Une bobine Bambu enregistrée affiche son poids (sans « ~ ») dans le détail | OK | Une seule valeur par bobine |
| T5-12 | Lien AMS : couleur/matière | Une bobine sans puce reconnue affiche un poids précédé de « ~ » | OK | présumé OK |
| T5-13 | Déconnexion | « Se déconnecter » vide l'inventaire et retire le compte | À faire | Non rapporté |
| T5-14 | Fluidité | Pas de gel de l'écran pendant la synchronisation ni le défilement de la liste | OK | fluidité correcte |

## Tests de longue durée (0.5.0)

| N° | Test | Résultat attendu | Résultat | Remarque |
|---|---|---|---|---|
| L-01 | Marche continue, 2 imprimantes + Tiger Studio | Pas de redémarrage, mémoire stable | OK | 2026-11-17 : 3 h 17 min sans redémarrage, 38 synchronisations Tiger (toutes les 5 min). Mémoire interne libre : 281 Ko → 276 Ko (≈ -5 Ko en 3 h), creux de 220 Ko pendant les sessions TLS, plus grand bloc minimal 176 Ko. X1C éteinte puis reconnectée à l'allumage. |

## Tests Affichage (0.5.1)

| N° | Test | Résultat attendu | Résultat | Remarque |
|---|---|---|---|---|
| A-01 | Compilation | « SUCCESS » | À faire | |
| A-02 | Réglages > Affichage | L'écran s'ouvre, affiche « Luminosité : 70 % » et « Extinction de l'écran : Jamais » | OK | |
| A-03 | Luminosité | « - » et « + » changent la luminosité de l'écran par pas de 10 %, entre 10 et 100 % | OK | |
| A-04 | Persistance | Après redémarrage (bouton Redémarrer), les deux réglages sont conservés | OK | |
| A-05 | Extinction | Avec « après 1 min » : après une minute sans toucher, l'écran s'éteint ; le premier toucher le rallume à la luminosité choisie sans actionner de bouton | À refaire | v1 : atténuation, remplacée par l'extinction (demande utilisateur) |
| A-06 | Extinction et impression | L'écran éteint continue de se mettre à jour (pas de gel) | OK | |
