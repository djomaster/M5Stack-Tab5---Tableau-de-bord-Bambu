# Processus de travail

## Versions

- Format `MAJEUR.MINEUR.CORRECTIF`. Pendant le développement, `0.X.Y`. La version 1.0.0 est la première version stable.
- La version est définie à un seul endroit : `src/version.h`. Elle s'affiche à l'écran au démarrage.
- Chaque version publiée a un tag git `vX.Y.Z` et une entrée datée dans `CHANGELOG.md`.

## Git

- Branche `main` : toujours dans un état qui compile et qui a été testé sur l'appareil.
- Une branche par chantier : `etape-2-mqtt`, `correctif-wifi`, etc. Fusion dans `main` seulement après test.
- Messages de commit : une ligne claire en français, au présent, préfixée par un type : `ajout:`, `correctif:`, `doc:`, `refactor:`. Exemple : `ajout: tuile imprimante avec progression`.
- Avant chaque commit, vérifier avec `git status` que `src/config.h` n'apparaît pas. Un fichier déjà suivi par git reste suivi même s'il est ensuite ajouté à `.gitignore` ; il faut alors le retirer avec `git rm --cached`.
- Jamais de secret dans git : mots de passe WiFi, codes d'accès des imprimantes, jetons. Ils vivent dans `src/config.h` et sur la carte SD, tous deux ignorés.

## Avant de publier une version

1. Mettre à jour `src/version.h`.
2. Dérouler les tests concernés de `TESTS.md` sur l'appareil et noter le résultat.
3. Renommer « Non publié » dans `CHANGELOG.md` en `[X.Y.Z] - date` et ouvrir une nouvelle section « Non publié ».
4. Mettre à jour `ROADMAP.md` (statuts) et `DECISIONS.md` si une décision a changé.
5. Commit, puis tag `vX.Y.Z`.

## Règles de documentation

- Un changement de comportement, de configuration ou d'architecture met à jour le document concerné dans le même commit.
- Une décision structurante s'ajoute à `DECISIONS.md`.
- Un test échoué ou un problème trouvé est noté dans `TESTS.md` avant d'être corrigé.

## Honnêteté sur l'état du code

Le code écrit mais non essayé sur l'appareil est marqué « À valider » dans `ROADMAP.md`. Il n'est « Terminé » qu'après réussite des tests correspondants.

## OneDrive

Le dossier du projet est dans OneDrive. Le dossier `.git` et le dossier `.pio` (fichiers de compilation, volumineux) peuvent ralentir la synchronisation ou la corrompre. Recommandation : exclure `.pio` de la synchronisation, ou déplacer le projet hors d'OneDrive et le sauvegarder via un dépôt distant privé.
