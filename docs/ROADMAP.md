# Feuille de route

Statuts : À faire · En cours · À valider (code écrit, pas testé sur l'appareil) · Terminé.

| Étape | Contenu | Version visée | Statut |
|---|---|---|---|
| 1 | Squelette, test matériel : écran, tactile, batterie, WiFi | 0.1.2 | Terminé |
| 2 | LVGL, MQTT d'une imprimante, tuile en direct, puis les autres | 0.2.0 | À faire |
| 3 | Vue détaillée, AMS, alertes sonores | 0.3.0 | À faire |
| 4 | Inventaire Tiger Studio | 0.4.0 | À faire |
| 5 | Caméra | 0.5.0 | À faire |
| 6 | Stabilisation, mémoire, tests longue durée, première version stable | 1.0.0 | À faire |

## Risques

| Risque | Effet | Parade |
|---|---|---|
| Révision d'écran récente non reconnue | Écran noir | Mettre à jour M5Unified/M5GFX, relever la révision (autocollant au dos) |
| Broches SDIO WiFi erronées | Pas de WiFi | Risque levé le 2026-11-17 (T1-07 réussi) ; broches dans `config.h` |
| Décodage H.264 trop lent | Caméra inutilisable | Réduire résolution et cadence ; étape 5 isolée |
| Plusieurs connexions TLS simultanées | Manque de mémoire | Mesure de la mémoire à chaque étape ; limite d'imprimantes à tester |
| H3C incompatible mode LAN | Imprimante non suivie | Vérifier avant achat |
| Mise à jour firmware Bambu | Changement du protocole | Noter la version firmware testée dans `TESTS.md` |

## Points ouverts

- Modèle exact et compatibilité mode LAN de la H3C.
- Adresse de l'API Tiger Studio à utiliser pour l'inventaire.
