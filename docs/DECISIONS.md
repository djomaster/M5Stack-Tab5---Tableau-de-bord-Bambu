# Journal des décisions

Chaque décision structurante est notée ici avec sa date et sa raison, pour ne pas la rediscuter sans le savoir. Une décision n'est jamais effacée : si elle change, on ajoute une nouvelle ligne qui la remplace.

| N° | Date | Décision | Raison | Statut |
|---|---|---|---|---|
| D-001 | 2026-11-16 | Appareil : M5Stack Tab5 | Écran 5" 720p, caméra, haut-parleur, batterie amovible | Actée |
| D-002 | 2026-11-16 | PlatformIO + Arduino (M5Unified/M5GFX), plateforme pioarduino | Même environnement VS Code que les autres projets ; le P4 exige pioarduino | Actée |
| D-003 | 2026-11-16 | Interface en LVGL | Interface riche et fluide en 1280 × 720 | Actée, pas encore intégrée |
| D-004 | 2026-11-16 | Pas de recherche MakerWorld ni d'envoi de fichiers | Pas d'API publique, protections anti-bot, maintenance fragile | Actée |
| D-005 | 2026-11-16 | Imprimantes pilotées en mode LAN (MQTT local) | Pas de dépendance au cloud Bambu | Actée |
| D-006 | 2026-11-16 | Caméra traitée en dernier | Décodage H.264 incertain sur le P4, risque technique le plus élevé | Actée |
| D-007 | 2026-11-16 | H3C : prise en charge conditionnée à la confirmation du modèle et de sa compatibilité mode LAN | Modèle non confirmé | Ouverte |
| D-008 | 2026-11-16 | Versions gérées par git (tags `vX.Y.Z`) et CHANGELOG, pas par dossiers copiés | Historique fiable, retour en arrière possible | Actée |
| D-009 | 2026-11-16 | Plateforme pioarduino installée depuis l'archive « stable », à figer sur une version précise dès que le build est validé | L'installation depuis un tag git a échoué sous Windows (T1-01) ; figer la version garantit des builds reproductibles | Provisoire |
| D-010 | 2026-11-16 | Plateforme pioarduino figée sur 54.03.21-2 (remplace D-009) | L'archive « stable » change et a exigé un PlatformIO Core plus récent que celui installé ; une version figée garantit des builds reproductibles | Actée, validée par un build (2026-11-16) |
| D-011 | 2026-11-17 | Configuration des imprimantes par un assistant à l'écran (recherche automatique, code d'accès, test, nom) plutôt que par un fichier à éditer | Plus simple pour l'utilisateur, pas de fichier de secrets à manipuler ; saisie manuelle en secours | Actée, à réaliser à l'étape 2 |
| D-012 | 2026-11-17 | WiFi configuré à l'écran dans un menu Réglages ; `src/config.h` supprimé | Plus aucun secret dans le code, changement de réseau sans recompiler | Réalisée et validée en 0.2.0 |
| D-013 | 2026-11-17 | LVGL 9.2.2 configuré par options de compilation (`LV_CONF_SKIP`) ; polices TinyTTF issues de Liberation Sans (OFL) embarquées | Pas de fichier `lv_conf.h` à maintenir ; les polices intégrées de LVGL n'ont pas les accents français | Actée, validée en 0.2.0 |
