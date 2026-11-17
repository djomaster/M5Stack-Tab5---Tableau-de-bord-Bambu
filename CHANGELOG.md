# Journal des changements

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/). Versionnage : [SemVer](https://semver.org/lang/fr/) (`MAJEUR.MINEUR.CORRECTIF`).

Règle : toute modification du code ou de la configuration ajoute une ligne dans « Non publié ». À chaque version, cette section devient une version datée. Voir `docs/PROCESSUS.md`.

## [Non publié]

### Modifié
- Clavier tactile pleine largeur en bas de l'écran (touches 110 x 68 px au lieu de 66 x 60). WiFi : deux modes (liste des réseaux, puis mot de passe avec clavier). Assistant imprimante et Tiger Studio : champs en haut, clavier en bas.
- Tiger Studio : le second identifiant RFID se lit dans le champ `twin_uid` (nom confirmé par la documentation d'intégration de TigerTag ; la clé Firebase, le projet et le chemin de l'inventaire étaient déjà conformes).
- Tiger Studio : en cas d'échec réseau, la console série détaille la cause (WiFi, DNS, TLS, mémoire).

## [0.5.0] - 2026-11-17 (à valider sur l'appareil, voir T5 dans `docs/TESTS.md`)

### Ajouté
- Compte Tiger Studio (Réglages > Tiger Studio) : connexion par e-mail et mot de passe. Le mot de passe n'est jamais conservé ; seul un jeton de renouvellement est gardé dans la mémoire de l'appareil (D-017).
- Onglet Inventaire : liste des bobines (couleur, marque, matière, nom de la couleur, poids restant), tri par matière, filtre par matière, total en kilos, poids en orange sous 100 g, synchronisation automatique toutes les 30 minutes et bouton « Synchroniser ».
- Rapprochement des bobines de l'AMS avec l'inventaire, dans la vue détaillée : par identifiant de puce RFID (poids sûr), sinon par couleur et matière (poids précédé de « ~ », approximatif). Pas de rapprochement pour la bobine externe.
- Clavier : couche « Symb. » (symboles utiles aux mots de passe).
- Barre de navigation à trois onglets : Accueil, Inventaire, Réglages.

### Modifié
- Menu Réglages : sept entrées plus compactes (ajout de Tiger Studio).
- Un seul clavier tactile, déplacé d'un écran à l'autre, au lieu d'un par écran.
- La tâche réseau Tiger Studio n'est créée que si un compte est connecté.

### Corrigé
- Redémarrages par watchdog : le journal et le fichier ELF montrent que PubSubClient attend la réponse de l'imprimante (CONNACK) en boucle active pendant 6 s, plus que le délai du watchdog (5 s). Délai ramené à 3 s, et la surveillance de la tâche d'inactivité du cœur 0 est retirée par sécurité.
- Tiger Studio : lecture du JSON tronquée (`IncompleteInput`, noms de marques et d'inventaire incomplets) : le corps de la réponse est maintenant lu en entier avant l'analyse.
- Connexion Tiger Studio : la mémoire interne ne suffit pas pour une session TLS tant que les imprimantes gardent les leurs ouvertes (`esp-aes : Failed to allocate`). Les connexions MQTT sont suspendues pendant une synchronisation Tiger Studio (quelques secondes), puis reprennent seules. Synchronisation automatique toutes les 30 minutes au lieu de 10.
- Une imprimante éteinte bloquait la tâche réseau 30 s par tentative (et empêchait les autres imprimantes de maintenir leur connexion) : un sondage TCP de 2,5 s précède maintenant la connexion TLS.
- Redémarrages par « Task watchdog » (tâche `bambu_net` qui monopolise le cœur 0 pendant une connexion TLS/MQTT, la bibliothèque PubSubClient attendant en boucle). Les tâches réseau passent à la priorité 0, égale à celle de la tâche d'inactivité, qui obtient ainsi du temps. La durée de chaque connexion MQTT est tracée.
- Allocation en PSRAM par défaut retirée : elle faisait échouer les accélérateurs matériels de chiffrement (`esp-sha`, `esp-aes : Failed to allocate`), qui exigent des tampons en mémoire interne, et empêchait même des connexions MQTT. Les piles réduites et le verrou TLS (ci-dessous) restent.
- Mémoire interne (essai 2) : l'allocation en PSRAM ne concerne pas la bibliothèque TLS, qui exige de la mémoire interne. Piles des tâches réseau réduites à 16 Ko (gain d'environ 20 Ko) ; une seule connexion TLS à la fois (imprimantes et Tiger Studio se partagent un verrou) ; Tiger Studio attend qu'un bloc de 24 Ko soit libre.
- Connexion Tiger Studio impossible (« Erreur réseau code -1 ») : la console montrait `SSL - Memory allocation failed` avec 40 Ko de mémoire interne libre sur 31 Mo de PSRAM. (Essai 1, abandonné : voir ci-dessus.)
- Redémarrages en boucle après la connexion des imprimantes (`assert failed: sdio_rx_get_buffer`, pilote WiFi du coprocesseur). Cause probable, non démontrée : manque de mémoire interne, la 0.5.0 ayant ajouté deux claviers (une centaine de boutons) et une tâche de 24 Ko. Réduction de ces deux postes ; la console série affiche maintenant la mémoire libre (« Mem ») au démarrage et toutes les 10 secondes pour confirmer.

### Limites connues
- Tiger Studio n'a pas d'API publique : la lecture passe par la base Firebase de TigerTag, avec les droits de l'utilisateur. Ce fonctionnement n'est pas documenté officiellement et peut changer.
- Connexion Google seule non prise en charge : un mot de passe TigerTag est nécessaire.
- Les noms de marques et de matières sont téléchargés au premier accès depuis le dépôt GitHub de TigerTag ; sans eux, l'inventaire affiche « Marque n » / « Matière n ».
- Les certificats TLS des serveurs ne sont pas vérifiés (comme pour les imprimantes).
- Maximum 400 bobines affichées.
- Code écrit sans compilation préalable.

## [0.4.0] - 2026-11-17

Validée sur le Tab5 avec la H2C et la X1C : 9 tests sur 11 réussis (`docs/TESTS.md`). Livrée avec T4-09 (son de fin d'impression) et T4-10 (son d'erreur) non testés : ils exigent une vraie impression et une vraie erreur, à refaire à l'occasion.

### Ajouté
- Vue détaillée d'une imprimante (toucher une tuile de l'accueil) : progression, fichier, couche, temps restant, températures de la buse et du plateau.
- Bobines de l'AMS : couleur, matière, pourcentage restant, bobine active en surbrillance (jusqu'à 4 unités).
- Alertes sonores par le haut-parleur du Tab5 : mélodie à la fin d'une impression, alarme à l'apparition d'une erreur ou d'un échec. Réglages > Alertes : activer/désactiver, volume (1 à 5), essais des deux sons. Réglages conservés après redémarrage.

### Modifié
- Écran pivoté de 180° (D-015).

### Corrigé
- Bobine externe (hors AMS) non détectée sur la X1C : lecture de `vt_tray` (et `vir_slot` pour la série H2), affichage dans « Bobine externe ».
- Sons désactivés : les essais et le volume émettaient encore un son ; silence complet désormais, avec un message sur l'écran Alertes.

- AMS HT (H2C) : reconnu par son identifiant (128 ou plus), affiché avec une seule bobine et libellé « AMS 3 HT » au lieu de quatre emplacements.
- Bobine externe : plus de pourcentage (l'imprimante ne le connaît pas, le poids ne peut pas être estimé). Le pourcentage est aussi masqué pour une bobine sans puce.

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
