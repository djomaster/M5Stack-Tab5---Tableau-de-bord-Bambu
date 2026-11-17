#pragma once
// Caméra des imprimantes (étape 6) : pour l'instant, sonde de diagnostic du flux RTSPS (voir docs/DECISIONS.md, D-024).
#include <Arduino.h>
#include "printers.h"

// Lance la sonde dans une tâche : connexion RTSPS (port 322), lecture du SDP, du profil H.264 et de ~8 s de flux.
void camera_probe_start(const PrinterCfg &cfg);
bool camera_probe_running();
// Compte rendu de la sonde (une ligne par information), mis à jour pendant son exécution.
String camera_probe_report();

// Image en direct (étape 6) : une image réduite 2 fois, décodée toutes les 2 s (voir D-026). La tâche coupe MQTT pendant l'affichage.
bool camera_view_start(const PrinterCfg &cfg);  // false si la tâche tourne déjà ou si la PSRAM manque
void camera_view_stop();
// Dernière image prête (RGB565, w x h, tampon valide jusqu'à l'image suivante) ; seq change à chaque nouvelle image.
bool camera_view_frame(const uint16_t **pix, int *w, int *h, uint32_t *seq);
uint32_t camera_view_decode_ms();
String camera_view_status();  // dernière ligne du compte rendu
