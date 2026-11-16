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
| T1-02 | Téléversement | Terminé sans erreur | Non testé | | |
| T1-03 | Affichage au démarrage | Titre et version visibles | Non testé | | |
| T1-04 | Tactile | Les coordonnées changent quand on touche | Non testé | | |
| T1-05 | Batterie | Niveau cohérent | Non testé | | |
| T1-06 | PSRAM | Environ 30 Mo libres affichés | Non testé | | |
| T1-07 | WiFi | Adresse IP affichée sous 30 s | Non testé | | |
