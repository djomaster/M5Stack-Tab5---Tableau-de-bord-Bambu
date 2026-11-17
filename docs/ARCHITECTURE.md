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

Les imprimantes sont configurées sur le Tab5 lui-même par un assistant (voir plus bas) et enregistrées dans la mémoire interne de l'appareil (nom, adresse IP, numéro de série, code d'accès). Il n'y a pas de fichier de configuration à éditer à la main. Aucun secret ne doit être écrit dans le code ni dans git. Le WiFi est enregistré de la même façon (mémoire de l'appareil, espace « wifi »).

## Assistant de configuration des imprimantes

Statut : prévu pour l'étape 2, non réalisé.

Parcours pour ajouter une imprimante, tout au toucher :

1. **Recherche** : le Tab5 cherche les imprimantes Bambu Lab sur le réseau local (annonce SSDP). Chaque imprimante trouvée affiche son modèle, son adresse IP et son numéro de série. Si rien n'est trouvé, saisie manuelle de l'adresse et du numéro de série.
2. **Code d'accès** : saisie du code du mode LAN (affiché sur l'écran de l'imprimante) avec le clavier à l'écran.
3. **Test** : connexion MQTT d'essai avec un résultat clair (réussi, code refusé, imprimante injoignable, mode LAN désactivé).
4. **Nom** : nom affiché sur la tuile (par défaut le modèle).
5. **Enregistrement** : l'imprimante apparaît dans l'accueil. Une imprimante se modifie ou se supprime depuis les réglages.

Contraintes :
- Le code d'accès est saisi masqué et jamais écrit dans les journaux série.
- Il est stocké dans la mémoire interne du Tab5 (en clair, comme sur l'imprimante) : quiconque a l'appareil en main peut le retrouver. À noter dans les réglages de sécurité.
- Le WiFi se configure aussi à l'écran, dans le menu Réglages (voir plus bas). `src/config.h` a disparu du projet (version 0.2.0).
- La découverte automatique reste à valider sur les trois modèles ; la saisie manuelle est toujours disponible.

## Menu Réglages

Statut : prévu pour l'étape 2, non réalisé.

| Entrée | Contenu |
|---|---|
| WiFi | Liste des réseaux détectés, saisie du mot de passe au clavier à l'écran, état de la connexion et adresse IP, oubli du réseau enregistré |
| Imprimantes | Liste des imprimantes configurées, ajout (assistant), modification, suppression |
| Alertes | Sons et événements à signaler (fin d'impression, erreur, filament vide) |
| Affichage | Luminosité et mise en veille de l'écran |
| À propos | Version du firmware, adresse IP, mémoire libre, niveau de batterie |

Le premier démarrage, sans WiFi enregistré, ouvre directement l'écran WiFi puis l'assistant d'ajout d'imprimante. Le mot de passe WiFi est stocké dans la mémoire interne du Tab5, avec les mêmes limites que les codes d'accès des imprimantes.

## Hors périmètre

Recherche MakerWorld et envoi de fichiers à l'imprimante : abandonnés (voir `DECISIONS.md`, D-004).
