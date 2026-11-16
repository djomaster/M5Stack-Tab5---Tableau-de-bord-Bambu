# Architecture

Statut : cible visée. Seule l'étape 1 (test matériel) existe dans le code. Voir `ROADMAP.md`.

## Objectif

Afficher sur un M5Stack Tab5 l'état de plusieurs imprimantes Bambu Lab (X1C, H2C, H3C envisagée) : progression, températures, AMS, caméra, alertes sonores, lien avec l'inventaire de bobines Tiger Studio.

## Matériel

| Élément | Détail |
|---|---|
| Appareil | M5Stack Tab5 |
| Processeur | ESP32-P4, 16 Mo de flash, 32 Mo de PSRAM |
| Radio | ESP32-C6, WiFi 6 uniquement, relié au P4 en SDIO (ESP-Hosted) |
| Écran | 5", 1280 × 720, tactile |
| Imprimantes | Bambu Lab X1C, H2C (existantes), H3C (achat envisagé) |

## Couches logicielles

1. **Matériel et plateforme** : M5Unified, M5GFX, WiFi.
2. **Données** : un client MQTT TLS par imprimante (mode LAN), un client HTTP pour Tiger Studio, un décodeur de flux caméra.
3. **Modèle** : une structure d'état par imprimante, alimentée par la couche données et lue par l'interface. L'interface ne parle jamais directement au réseau.
4. **Interface** : LVGL, une tuile par imprimante, une vue détaillée par imprimante.
5. **Alertes** : événements (fin d'impression, erreur, filament vide) transformés en sons.

## Communication avec les imprimantes

- MQTT TLS, port 8883, utilisateur `bblp`, mot de passe = code d'accès de l'imprimante, sujet de lecture `device/<numéro de série>/report`.
- Caméra : flux RTSPS (port 322) sur la X1C. À vérifier sur la H2C. Le décodage H.264 sur le P4 reste à valider.
- Le mode LAN doit être activé sur chaque imprimante.

## Configuration

Les imprimantes seront décrites dans un fichier JSON sur carte SD (nom, adresse IP, numéro de série, code d'accès). Aucun secret ne doit être écrit dans le code ni dans git. Le fichier `src/config.h` (WiFi) est ignoré par git ; `src/config.example.h` sert de modèle.

## Hors périmètre

Recherche MakerWorld et envoi de fichiers à l'imprimante : abandonnés (voir `DECISIONS.md`, D-004).
