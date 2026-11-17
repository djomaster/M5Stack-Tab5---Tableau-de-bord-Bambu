#pragma once
#include <Arduino.h>
#include "printers.h"

enum class Link { Off, Connecting, Connected, AuthFailed, Unreachable };

// Une bobine de l'AMS.
struct Tray {
  bool present = false;
  uint32_t color = 0;  // RRGGBB
  String type;         // PLA, PETG...
  int remain = -1;     // pourcentage restant, -1 si inconnu
};

static const int MAX_TRAYS = 16;  // 4 unités AMS de 4 bobines

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
  int tray_now = -1;  // bobine active : unité*4 + bobine ; 254 = bobine externe ; -1 = inconnu
  Tray trays[MAX_TRAYS];
};

void bambu_start();
void bambu_reload();
bool bambu_state(int i, PrinterState &out);

void bambu_test_start(const PrinterCfg &cfg);
bool bambu_test_status(PrinterState &out);
void bambu_test_stop();
