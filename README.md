# Tableau de bord Bambu Lab - M5Stack Tab5

Tableau de bord multi-imprimantes (X1C, H2C, H3C envisagée) sur M5Stack Tab5 : suivi d'impression en direct, AMS, caméra, alertes sonores, inventaire de bobines Tiger Studio.

**Version actuelle : 0.1.0** (squelette, non testé sur l'appareil). Voir `CHANGELOG.md`.

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
2. Renseigner le WiFi dans `src/config.h` (modèle : `src/config.example.h`).
3. Compiler et téléverser l'environnement `tab5`.

Détails et dépannage : `docs/PROCEDURES.md`.

## Secrets

`src/config.h` et le futur fichier de configuration des imprimantes contiennent des mots de passe et codes d'accès : ils ne vont jamais dans git.
