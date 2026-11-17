# Journal des changements

Format : [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/). Versionnage : [SemVer](https://semver.org/lang/fr/) (`MAJEUR.MINEUR.CORRECTIF`).

Règle : toute modification du code ou de la configuration ajoute une ligne dans « Non publié ». À chaque version, cette section devient une version datée. Voir `docs/PROCESSUS.md`.

## [Non publié]

### Ajouté
- Caméra, étape 1 : bouton « Caméra » dans le détail d'une imprimante. Sonde de diagnostic (`src/camera.cpp`) : connexion RTSPS (port 322, authentification Digest ou Basic), lecture du SDP, profil et résolution H.264, CABAC, images B, cadence et pertes sur 8 s, verdict de compatibilité avec le décodeur esp_h264. Aucune image n'est affichée. Analyseur de paramètres H.264 vérifié sur la machine de développement (High 1920x1080 et Baseline 320x240). Premier essai sur l'appareil : TLS, authentification Digest, DESCRIBE et SETUP fonctionnent sur la X1C et la H2C ; après correction (trames entrelacées avant la réponse au PLAY, en-tête Range retiré), la sonde lit 8 s de flux sur les deux imprimantes : H.264 High 4.1, CABAC, sans image B, 30 images/s, 0 perte, donc incompatible avec le décodeur esp_h264 (D-025).
- Caméra, étape 2 (option 1, D-026) : décodeur H.264 « intra seul » écrit en C++ (`src/h264i.cpp`, `h264i.h`, `h264i_tables.h`) : profil High, CABAC, prédiction intra 4x4/8x8/16x16, transformée 8x8, plusieurs tranches, sans dégroupage. Vérifié sur PC : image identique à l'octet près à celle de ffmpeg (`-skip_loop_filter all`) sur 9 flux d'essai (`tools/h264i_check.py`, `tools/cabac_selftest.cpp`, `tools/h264i_test.cpp`). La sonde reconstitue les unités NAL (FU-A, STAP-A), décode les 3 premières images clés et affiche le temps de décodage, la taille et la mémoire PSRAM libre (pile de la tâche portée à 20 Ko). Mesuré sur la tablette : 0,74 à 0,83 s par image sur la X1C (1760x1080) et 0,96 à 1,02 s sur la H2C (1680x1080), 3 images sur 3 décodées sur chaque imprimante, sans fuite de mémoire interne ; aucune image affichée.
- Caméra, étape 3 : le bouton « Caméra » ouvre maintenant l'image en direct de l'imprimante : une image clé décodée toutes les 2 s (puis 1 par seconde, essai : délai mesuré entre les débuts de décodage, tolérance 150 ms), réduite de moitié (environ 880x540, couleurs BT.601), double tampon en PSRAM, état et temps de décodage à droite. Le flux reste ouvert tant que l'écran est affiché et se ferme avec « Fermer » ; MQTT est suspendu pendant ce temps (comme pour la sonde) et repris à la fermeture. Le compte rendu de la sonde reste disponible sur la liaison série. À tester sur l'appareil, non compilé ici.
- Caméra, correction : sur la X1C le flux s'arrêtait après environ 12 s (« plus de données depuis 10 s », 3 essais sur 3, 0 perte, 0 erreur de décodage) alors que le décodage de 0,8 à 1 s bloquait la lecture du flux. Le décodage se fait maintenant dans une tâche séparée (cœur 1, priorité basse) ; la lecture du flux (priorité plus haute) ne fait que copier l'image clé retenue, et saute les images clés tant que le décodeur est occupé (donc environ 1 image toutes les 2 s sur la H2C, 1 par seconde sur la X1C). Un message OPTIONS de maintien de session est envoyé toutes les 5 s. Le message d'arrêt indique maintenant la durée et le nombre de paquets reçus. À tester.
- Caméra, vitesse du décodeur : image en direct constatée à 1 image toutes les 2 s sur la X1C et 1 toutes les 4 à 5 s sur la H2C (le décodage de 0,8 à 1 s fait sauter des images clés). Optimisations du décodeur : prédiction intra sans test de mode par pixel (modes en paramètre de modèle, valeur DC calculée une seule fois), lecture des bits CABAC par réservoir 64 bits avec renormalisation en une opération, compilation du fichier en O3 (Arduino compile en -Os). Sur PC, en -Os : 11,7 ms puis 4,0 ms par image (≈ 3 fois plus rapide) ; résultat toujours identique à ffmpeg sur les 9 flux d'essai. Gain sur la tablette à mesurer (C-11).
- Caméra, correction de cœur : sur la vidéo de l'écran, « Décodage » affichait 2729 ms (X1C) et 4601 ms (H2C), contre 0,8 à 1 s mesurés quand le décodage tournait sur le cœur 0. La tâche de décodage est donc remise sur le cœur 0 (priorité 0, sous la lecture du flux), le cœur 1 étant presque entièrement occupé par la boucle LVGL. Image constatée sur l'appareil : correcte sur la H2C, dominante cyan sur la X1C (à vérifier avec l'éclairage de la chambre).
- Compilation automatique sur GitHub (`.github/workflows/build.yml`, GitHub Actions) : vérification à chaque envoi sur `main`, firmware joint aux releases (fichier unique `-complet.bin` + fichiers séparés). Script `tools/merge_bin.py` qui assemble le fichier unique à chaque build (sans jamais faire échouer la compilation). Script validé en local (build SUCCESS, fichier unique de 1,7 Mo créé, bootloader + partitions + boot_app0 + application) ; première exécution sur GitHub et flashage du fichier unique à vérifier. Le workflow peut aussi joindre les fichiers à une release déjà publiée (Run workflow avec le tag).

## [0.5.1] - 2026-11-17 (validée sauf A-07, réveil par alerte, à tester avec une impression)

### Ajouté
- L'écran éteint se rallume à la fin d'une impression et à l'apparition d'une erreur (même si les sons sont désactivés).
- Réglages > Affichage : luminosité de l'écran (10 à 100 %) et extinction automatique de l'écran après 1, 5 ou 15 minutes d'inactivité ; le premier toucher le rallume sans actionner ce qui se trouve dessous. Réglages conservés après redémarrage.

## [0.5.0] - 2026-11-17 (validée sur l'appareil, sauf T5-13 « Se déconnecter » à tester)

### Ajouté
- Compte Tiger Studio (Réglages > Tiger Studio) : connexion par e-mail et mot de passe. Le mot de passe n'est jamais conservé ; seul un jeton de renouvellement est gardé dans la mémoire de l'appareil (D-017).
- Onglet Inventaire : liste des bobines (couleur, marque, matière, nom de la couleur, poids restant), tri par matière, filtre par matière, total en kilos, poids en orange sous 100 g, synchronisation automatique toutes les 30 minutes et bouton « Synchroniser ».
- Rapprochement des bobines de l'AMS avec l'inventaire, dans la vue détaillée : par identifiant de puce RFID (poids sûr), sinon par couleur et matière (poids précédé de « ~ », approximatif). Pas de rapprochement pour la bobine externe.
- Clavier : couche « Symb. » (symboles utiles aux mots de passe).
- Barre de navigation à trois onglets : Accueil, Inventaire, Réglages.

### Modifié
- Inventaire : la couleur des bobines TigerData (puce Tiger sans `online_color_list`) est lue dans `color_r/g/b`, et le nom de couleur dans `message` (corrige les deux CC3D HIPS affichées en noir). Le diagnostic « document brut » est retiré.
- WiFi : SSID et adresse IP mis en cache à la connexion au lieu d'être relus chaque seconde (le WiFi du P4 passe par un coprocesseur ; piste pour les redémarrages watchdog).
- Inventaire : trois filtres (Marque, Matière, Couleur) à choisir dans une liste avec le nombre de bobines ; ils se combinent, bouton « Réinitialiser ». La couleur est classée par famille (Noir, Blanc, Gris, Rouge, Orange, Marron, Jaune, Vert, Cyan, Bleu, Violet, Rose) d'après le code RVB.
- Réglages : bouton « Redémarrer la tablette » (confirmation en deux appuis).
- Inventaire : sauvegardé en mémoire flash (LittleFS, `/tiger.tsv`) avec les noms de marques et de matières ; il réapparaît dès le démarrage, avant la première synchronisation.
- Diagnostic : les 16 dernières étapes sont conservées en mémoire RTC et affichées au démarrage suivant un plantage (« Avant le redemarrage, dernieres etapes : … »).
- Inventaire : filtre par matière en pastilles cliquables (« Toutes » + une par matière), à la place du bouton cyclique.
- Inventaire : synchronisation automatique toutes les 30 s tant que l'écran Inventaire est affiché, 5 min sinon ; la liste n'est redessinée que si le contenu change.
- Détail imprimante : une seule valeur par bobine (poids Tiger, « env. » si approximatif ; sinon le pourcentage de l'imprimante).
- Poids de l'inventaire estimés par couleur : préfixe « env. » au lieu de « ~ » (le tilde ressemblait à un signe moins).
- Diagnostic : cause du dernier redémarrage au démarrage, mémoire après chaque étape d'initialisation, et ligne « Mem CHUTE » avec les dernières étapes quand la mémoire interne chute de plus de 12 Ko en 200 ms.
- Clavier : bouton « Réduire » (en bas à droite) qui masque le clavier ; toucher de nouveau un champ de saisie le ré-affiche.
- Clavier tactile pleine largeur en bas de l'écran (touches 110 x 68 px au lieu de 66 x 60). WiFi : deux modes (liste des réseaux, puis mot de passe avec clavier). Assistant imprimante et Tiger Studio : champs en haut, clavier en bas.
- Tiger Studio : le second identifiant RFID se lit dans le champ `twin_uid` (nom confirmé par la documentation d'intégration de TigerTag ; la clé Firebase, le projet et le chemin de l'inventaire étaient déjà conformes).
- Tiger Studio : en cas d'échec réseau, la console série détaille la cause (WiFi, DNS, TLS, mémoire).
- Menu Réglages : sept entrées plus compactes (ajout de Tiger Studio).
- Un seul clavier tactile, déplacé d'un écran à l'autre, au lieu d'un par écran.
- La tâche réseau Tiger Studio n'est créée que si un compte est connecté.

### Corrigé
- Mémoire interne : tous les objets LVGL (écrans, listes, clavier) sont maintenant alloués en PSRAM (`src/lv_mem_psram.cpp`, `LV_STDLIB_CUSTOM`) ; la mémoire interne reste disponible pour le WiFi, TLS et les reconnexions MQTT.
- Mémoire interne : après une synchronisation Tiger Studio réussie (74 bobines), la reconnexion de l'imprimante échouait (`SSL - Memory allocation failed`, mémoire interne tombée à 5 Ko). L'interface LVGL (plus de 150 objets, dont le clavier) est maintenant construite en PSRAM ; une connexion MQTT n'est tentée que si un bloc interne de 45 Ko est libre (sinon nouvel essai 8 s plus tard, sans gaspiller ni fragmenter la mémoire). La console affiche la mémoire avant l'interface pour mesurer ce que l'interface coûtait.
- Tiger Studio consommait plus de 100 Ko de mémoire interne pendant une synchronisation (la mémoire tombait à 41 Ko et la connexion d'une imprimante se fermait). Les réponses sont lues dans un tampon en PSRAM et les documents JSON utilisent la PSRAM ; les imprimantes sont suspendues dès que le plus gros bloc interne libre est inférieur à 140 Ko ; chaque requête Tiger est tracée (octets, durée, mémoire).
- Tiger Studio ne suspend plus les imprimantes que si la mémoire interne est insuffisante (bloc libre < 80 Ko) ; première synchronisation 30 s après le démarrage, pour ne pas concurrencer la connexion des imprimantes. Délai d'attente MQTT remis à 8 s (3 s provoquait des échecs « rc=-4 » quand le processeur était partagé).
- Redémarrages par watchdog : le journal et le fichier ELF montrent que PubSubClient attend la réponse de l'imprimante (CONNACK) en boucle active pendant 6 s, plus que le délai du watchdog (5 s). Délai ramené à 3 s, et la surveillance de la tâche d'inactivité du cœur 0 est retirée par sécurité (par la reconfiguration officielle du watchdog ; un premier essai par désinscription directe inondait la console de « task not found »).
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
