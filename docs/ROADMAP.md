# Feuille de route

Statuts : À faire · En cours · À valider (code écrit, pas testé sur l'appareil) · Terminé.

| Étape | Contenu | Version visée | Statut |
|---|---|---|---|
| 1 | Squelette, test matériel : écran, tactile, batterie, WiFi | 0.1.2 | Terminé |
| 2 | LVGL, accueil (données d'exemple), menu Réglages, configuration WiFi à l'écran | 0.2.0 | Terminé |
| 3 | Assistant de configuration des imprimantes, MQTT, tuiles en direct | 0.3.0 | Terminé |
| 4 | Vue détaillée, AMS, alertes sonores | 0.4.0 | Terminé (T4-09 et T4-10 à refaire) |
| 5 | Inventaire Tiger Studio et lien avec l'AMS | 0.5.0 | Terminé (T5-13 à tester) |
| 6 | Caméra | 0.6.0 | En cours : sonde faite ; décodeur intra seul écrit et vérifié sur PC, chronométrage sur la tablette à faire, puis affichage (D-026) |
| 7 | Stabilisation, mémoire, tests longue durée, première version stable | 1.0.0 | À faire |

## Risques

| Risque | Effet | Parade |
|---|---|---|
| Révision d'écran récente non reconnue | Écran noir | Mettre à jour M5Unified/M5GFX, relever la révision (autocollant au dos) |
| Broches SDIO WiFi erronées | Pas de WiFi | Risque levé le 2026-11-17 (T1-07 réussi) ; broches dans `wifi_manager.cpp` |
| Décodage H.264 trop lent | Caméra inutilisable | Réduire résolution et cadence ; étape 5 isolée |
| Plusieurs connexions TLS simultanées | Manque de mémoire | Mesure de la mémoire à chaque étape ; limite d'imprimantes à tester |
| H3C incompatible mode LAN | Imprimante non suivie | Vérifier avant achat |
| Mise à jour firmware Bambu | Changement du protocole | Noter la version firmware testée dans `TESTS.md` |

## Points ouverts

- Découverte automatique des imprimantes : à valider sur X1C, H2C et H3C.
- Modèle exact et compatibilité mode LAN de la H3C.
- Adresse de l'API Tiger Studio à utiliser pour l'inventaire.
