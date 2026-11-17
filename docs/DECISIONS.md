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
| D-011 | 2026-11-17 | Configuration des imprimantes par un assistant à l'écran (recherche automatique, code d'accès, test, nom) plutôt que par un fichier à éditer | Plus simple pour l'utilisateur, pas de fichier de secrets à manipuler ; saisie manuelle en secours | Réalisée en 0.3.0, validé en 0.3.0 |
| D-012 | 2026-11-17 | WiFi configuré à l'écran dans un menu Réglages ; `src/config.h` supprimé | Plus aucun secret dans le code, changement de réseau sans recompiler | Réalisée et validée en 0.2.0 |
| D-013 | 2026-11-17 | LVGL 9.2.2 configuré par options de compilation (`LV_CONF_SKIP`) ; polices TinyTTF issues de Liberation Sans (OFL) embarquées | Pas de fichier `lv_conf.h` à maintenir ; les polices intégrées de LVGL n'ont pas les accents français | Actée, validée en 0.2.0 |
| D-014 | 2026-11-17 | MQTT dans une tâche réseau séparée (cœur 0), état partagé protégé par un verrou ; bibliothèques PubSubClient et ArduinoJson (filtre JSON) ; certificat de l'imprimante non vérifié ; limite de 3 imprimantes | La connexion TLS bloque plusieurs secondes et ne doit pas figer l'écran ; les rapports Bambu sont volumineux ; certificat auto-signé sur réseau local ; mémoire à mesurer | Actée, validée en 0.3.0 |
| D-015 | 2026-11-17 | Écran pivoté de 180° (rotation 3 au lieu de 1) | Demande de l'utilisateur, selon l'installation du Tab5 | Actée, validée en 0.4.0 |
| D-016 | 2026-11-17 | Alertes sonores par le haut-parleur intégré (M5Unified), mélodies courtes, détection par changement d'état (RUNNING vers FINISH, apparition d'une erreur) | Pas de dépendance externe ; pas de son au premier rapport pour éviter les fausses alertes | Actée, validée en 0.4.0 |
| D-017 | 2026-11-17 | Inventaire lu dans la base Firebase de TigerTag (Authentication + Firestore, `users/{uid}/inventory`) avec le compte de l'utilisateur, en e-mail + mot de passe ; seul le jeton de renouvellement est conservé ; noms de marques/matières téléchargés depuis le dépôt GitHub de TigerTag ; rapprochement AMS par puce RFID puis par couleur et matière | Tiger Studio n'a pas d'API publique ; c'est la même base que l'application officielle et le projet TigerScale de l'utilisateur ; pas de recopie de tables dont l'exactitude ne peut pas être vérifiée | Actée, à valider (T5) ; fragile si TigerTag change son schéma |
| D-018 | 2026-11-17 | Tous les objets LVGL sont alloués en PSRAM (`src/lv_mem_psram.cpp`, `LV_STDLIB_CUSTOM`) | La mémoire interne (≈ 420 Ko) doit rester disponible pour le WiFi, TLS et le DMA ; l'interface en prenait 157 Ko | Actée, validée (3 h 17 min sans fuite) |
| D-019 | 2026-11-17 | Inventaire et noms de marques/matières mis en cache dans LittleFS (`/tiger.tsv`) | L'inventaire doit réapparaître au démarrage, hors ligne | Actée, validée (T5-10) |
| D-020 | 2026-11-17 | Les 16 dernières étapes du programme sont gardées en mémoire RTC et affichées après un plantage | Diagnostiquer les redémarrages watchdog sans débogueur | Actée |
| D-021 | 2026-11-17 | SSID et IP du WiFi lus une seule fois à la connexion | Le WiFi du P4 passe par un coprocesseur (SDIO) ; éviter les échanges inutiles chaque seconde | Actée, effet sur les watchdog à confirmer |
| D-022 | 2026-11-17 | Caméra reportée à une date indéterminée | Demande de l'utilisateur | Actée |
