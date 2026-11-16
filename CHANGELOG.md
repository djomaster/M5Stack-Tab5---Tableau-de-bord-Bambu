# Journal des changements

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/). Versionnage : [SemVer](https://semver.org/lang/fr/) (`MAJEUR.MINEUR.CORRECTIF`).

Règle : toute modification du code ou de la configuration ajoute une ligne dans « Non publié ». À chaque version, cette section devient une version datée. Voir `docs/PROCESSUS.md`.

## [Non publié]

### Corrigé
- `src/config.h` (WiFi) était suivi par git depuis le commit « première compilation », alors qu'il doit rester local. Il n'est plus suivi ; le fichier local est conservé et le modèle reste `src/config.example.h`. Aucun mot de passe réel n'avait été commité (vérifié sur tout l'historique).

### Validé
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
