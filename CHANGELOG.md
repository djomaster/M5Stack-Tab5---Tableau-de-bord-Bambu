# Journal des changements

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/). Versionnage : [SemVer](https://semver.org/lang/fr/) (`MAJEUR.MINEUR.CORRECTIF`).

Règle : toute modification du code ou de la configuration ajoute une ligne dans « Non publié ». À chaque version, cette section devient une version datée. Voir `docs/PROCESSUS.md`.

## [Non publié]

## [0.4.0] - 2026-11-17 (à valider sur l'appareil, voir T4 dans `docs/TESTS.md`)

### Ajouté
- Vue détaillée d'une imprimante (toucher une tuile de l'accueil) : progression, fichier, couche, temps restant, températures de la buse et du plateau.
- Bobines de l'AMS : couleur, matière, pourcentage restant, bobine active en surbrillance (jusqu'à 4 unités).
- Alertes sonores par le haut-parleur du Tab5 : mélodie à la fin d'une impression, alarme à l'apparition d'une erreur ou d'un échec. Réglages > Alertes : activer/désactiver, volume (1 à 5), essais des deux sons. Réglages conservés après redémarrage.

### Modifié
- Écran pivoté de 180° (D-015).

### Corrigé
- Bobine externe (hors AMS) non détectée sur la X1C : lecture de `vt_tray` (et `vir_slot` pour la série H2), affichage dans « Bobine externe ».
- Sons désactivés : les essais et le volume émettaient encore un son ; silence complet désormais, avec un message sur l'écran Alertes.

### Limites connues
- Les codes d'erreur restent affichés bruts.
- Lecture de l'AMS écrite d'après le format des rapports Bambu ; à confirmer sur la H2C (AMS 2 Pro / HT) et la X1C.
- Le premier rapport reçu après connexion sert de référence : une impression terminée pendant que le Tab5 était éteint ne déclenche pas de son.

## [0.3.0] - 2026-11-17

Validée sur le Tab5 avec la H2C et la X1C : tests T3-01 à T3-12 réussis (`docs/TESTS.md`).

### Ajouté
- Assistant d'ajout d'imprimante en 3 étapes : recherche sur le réseau (annonces SSDP) ou saisie manuelle, adresse IP + numéro de série + code d'accès LAN avec test de connexion, puis nom (D-011).
- Écran Réglages > Imprimantes : liste, état de connexion, suppression avec confirmation. Maximum 3 imprimantes.
- Connexion MQTT (TLS, port 8883) à chaque imprimante dans une tâche réseau séparée ; lecture de l'état d'impression (état, pourcentage, temps restant, couche, températures, fichier, code d'erreur).
- Accueil : une tuile en direct par imprimante, à la place des données d'exemple.
- Les imprimantes (dont le code d'accès) sont enregistrées dans la mémoire de l'appareil, jamais dans le code ni dans git.

### Modifié
- Découpage de l'interface : `ui_common` (composants, clavier), `ui` (accueil, réglages, WiFi), `ui_printers` (liste, assistant).
- Clavier : ajout de « Verr. Maj » (majuscules continues, utile pour le numéro de série) et « Vider ».
- Nouvelles dépendances : PubSubClient 2.8, ArduinoJson 7.

### Corrigé
- Programme trop gros pour la table de partitions par défaut (1,4 Mo pour 1,25 Mo disponibles) : passage à la table `default_16MB.csv` (application jusqu'à 6,5 Mo). Le message « No such option: --ng » de esp-idf-size est un avertissement sans effet sur le firmware.

### Corrigé
- Test de connexion : « Connecté » puis aucune donnée reçue. Le numéro de série est maintenant converti en majuscules (le sujet MQTT distingue majuscules et minuscules), le tampon de réception passe à 64 Ko, le maximum de PubSubClient (la taille est stockée sur 16 bits : 128 Ko échouait et laissait un tampon de 256 octets, donc tous les rapports étaient ignorés) et la console série trace l'abonnement et les messages reçus pour le diagnostic.

### Limites connues
- Les codes d'erreur sont affichés bruts (pas encore de traduction).
- La compatibilité de la H2C et d'une future H3C reste à confirmer.

## [0.2.0] - 2026-11-17

Validée sur le Tab5 : tests T2-01 à T2-10 réussis (`docs/TESTS.md`).

### Ajouté
- Interface LVGL 9.2.2 en 1280 × 720 : accueil avec données d'exemple (X1C en impression, H2C en erreur, tuile « Ajouter une imprimante »), menu Réglages, écran À propos, barre d'état (WiFi, batterie) et barre de navigation.
- Configuration du WiFi à l'écran : recherche des réseaux, saisie du mot de passe (clavier AZERTY tactile avec Maj, Afficher/Masquer), connexion, enregistrement dans la mémoire de l'appareil et reconnexion automatique au démarrage. Au premier démarrage, l'écran WiFi s'ouvre directement.
- Accents français : polices Liberation Sans (licence OFL) embarquées, générées par `tools/make_font_headers.py` (D-013).

### Modifié
- Découpage du code : `display_port`, `fonts`, `wifi_manager`, `ui`, `main`.

### Supprimé
- `src/config.h` et `src/config.example.h` : le WiFi ne se configure plus par fichier (D-012).

### Corrigé
- Échec de l'édition de liens (« can't link soft-float modules with single-float modules ») : les fichiers assembleur ARM de LVGL (Helium, Neon) sont exclus par `tools/skip_lvgl_asm.py` et l'accélération assembleur est désactivée.

### Limites connues
- Les écrans Imprimantes, Alertes et Affichage affichent seulement « Disponible dans une prochaine version ». Aucune connexion aux imprimantes pour l'instant.

### Corrigé
- `src/config.h` (WiFi) était suivi par git depuis le commit « première compilation », alors qu'il doit rester local. Il n'est plus suivi (puis supprimé en 0.2.0). Aucun mot de passe réel n'avait été commité (vérifié sur tout l'historique).

### Validé
- Étape 1 sur le Tab5 : téléversement, affichage de la version, tactile, batterie et PSRAM (T1-02 à T1-06). WiFi (T1-07) : réussi, adresse IP obtenue. L'étape 1 est terminée.
- Compilation de l'environnement `tab5` réussie avec la plateforme 54.03.21-2 (T1-01). Téléversement et tests sur l'appareil (T1-02 à T1-07) : en attente.

## [0.1.2] - 2026-11-16

### Corrigé
- La plateforme « stable » (55.3.312) est refusée : elle exige PlatformIO Core 6.2.0 ou plus, alors que 6.2.0-post.1 est installé. La plateforme pioarduino est figée sur la version 54.03.21-2, qui ne dépend pas d'un PlatformIO plus récent. À valider (T1-01).

## [0.1.1] - 2026-11-16

### Corrigé
- Échec de l'installation de la plateforme au premier lancement (erreur `FileNotFoundError` pendant l'étape `postinstall`, Windows). La plateforme pioarduino est désormais installée depuis l'archive publiée « stable » au lieu d'un dépôt git épinglé. Correctif à valider (voir `docs/TESTS.md`, T1-01).

## [0.1.0] - 2026-11-16

Premier squelette du projet, **non testé sur l'appareil**.

### Ajouté
- Projet PlatformIO pour le M5Stack Tab5 (ESP32-P4, plateforme pioarduino).
- Programme de test matériel : écran, tactile, niveau de batterie, PSRAM, WiFi via l'ESP32-C6.
- Affichage de la version du firmware à l'écran.
- Documentation : architecture, décisions, feuille de route, processus, procédures, plan de tests.
