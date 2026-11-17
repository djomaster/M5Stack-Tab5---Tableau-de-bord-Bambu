#pragma once
#include <Arduino.h>
#include "printers.h"

enum class Link { Off, Connecting, Connected, AuthFailed, Unreachable };

// État d'une imprimante, tel que reçu par MQTT (copie sûre à lire depuis l'interface).
struct PrinterState {
  Link link = Link::Off;
  bool got_report = false;  // au moins un rapport reçu
  String gcode;             // IDLE, RUNNING, PAUSE, FINISH, FAILED, PREPARE...
  String file;
  int percent = 0;
  int remaining_min = -1;
  int layer = 0;
  int total_layers = 0;
  int error = 0;
  float nozzle = 0;
  float bed = 0;
};

// Lance la tâche réseau (une connexion MQTT par imprimante enregistrée).
void bambu_start();
// À appeler après un ajout ou une suppression d'imprimante.
void bambu_reload();
// Copie l'état de l'imprimante i ; false si elle n'existe pas encore côté réseau.
bool bambu_state(int i, PrinterState &out);

// Test de connexion de l'assistant (connexion temporaire, non enregistrée).
void bambu_test_start(const PrinterCfg &cfg);
bool bambu_test_status(PrinterState &out);
void bambu_test_stop();
