# Tableau de bord Bambu Lab - M5Stack Tab5

Tableau de bord multi-imprimantes (X1C, H2C, H3C envisagée) sur M5Stack Tab5 : suivi d'impression en direct, AMS, caméra, alertes sonores, inventaire de bobines Tiger Studio.

**Dernière version validée : 0.4.0** (testée sur Tab5 avec X1C et H2C). **En cours : 0.5.0** (inventaire Tiger Studio, à valider). Voir `CHANGELOG.md`.

## Fonctions

| Fonction | Version | État |
|---|---|---|
| Écran tactile LVGL, WiFi réglé à l'écran | 0.2.0 | validé |
| Assistant d'ajout d'imprimantes (découverte SSDP ou saisie), MQTT en direct, tuiles d'accueil | 0.3.0 | validé |
| Vue détaillée, AMS / AMS HT / bobine externe, alertes sonores (fin d'impression, erreur) | 0.4.0 | validé (sons de fin et d'erreur à retester sur une vraie impression) |
| Inventaire Tiger Studio, lien avec l'AMS | 0.5.0 | à valider |
| Caméra | 0.6.0 | prévu |
| Stabilisation, H3C | 1.0.0 | prévu |

## Documentation

| Document | Contenu |
|---|---|
| `docs/ARCHITECTURE.md` | Objectif, matériel, couches logicielles, communication avec les imprimantes |
| `docs/DECISIONS.md` | Journal des décisions et leurs raisons |
| `docs/ROADMAP.md` | Étapes, statuts, risques, points ouverts |
| `docs/PROCESSUS.md` | Versions, git, publication, règles de documentation |
| `docs/PROCEDURES.md` | Compiler, téléverser, activer le mode LAN, sauvegarder |
| `docs/TESTS.md` | Plan de tests et résultats par version |
| `CHANGELOG.md` | Historique des changements par version |

## Démarrage rapide

1. Ouvrir ce dossier dans VS Code (extension PlatformIO).
2. Le WiFi, les imprimantes (jusqu'à 3), les alertes et le compte Tiger Studio se règlent sur le Tab5 lui-même : menu Réglages (aucun fichier à éditer).
3. Compiler et téléverser l'environnement `tab5`.

Détails et dépannage : `docs/PROCEDURES.md`.

## Secrets

Les mots de passe WiFi, codes d'accès des imprimantes et jeton Tiger Studio (le mot de passe n'est jamais conservé) sont enregistrés dans la mémoire de l'appareil, jamais dans le code ni dans git.
