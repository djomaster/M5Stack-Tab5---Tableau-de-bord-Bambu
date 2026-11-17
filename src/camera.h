#pragma once
// Caméra des imprimantes (étape 6) : pour l'instant, sonde de diagnostic du flux RTSPS (voir docs/DECISIONS.md, D-024).
#include <Arduino.h>
#include "printers.h"

// Lance la sonde dans une tâche : connexion RTSPS (port 322), lecture du SDP, du profil H.264 et de ~8 s de flux.
void camera_probe_start(const PrinterCfg &cfg);
bool camera_probe_running();
// Compte rendu de la sonde (une ligne par information), mis à jour pendant son exécution.
String camera_probe_report();
