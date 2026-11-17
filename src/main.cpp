// Tableau de bord Bambu Lab - M5Stack Tab5
// Version 0.4.0 : vue détaillée, bobines de l'AMS, alertes sonores.

#include <M5Unified.h>
#include <lvgl.h>
#include "alerts.h"
#include "bambu.h"
#include "display_port.h"
#include "fonts.h"
#include "printers.h"
#include "ui.h"
#include "version.h"
#include "wifi_manager.h"

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.printf("%s v%s\n", FW_NAME, FW_VERSION);

  printers_load();
  alerts_load();
  wifi_begin();
  bambu_start();  // crée le verrou et la tâche réseau avant que l'interface ne les interroge
  display_port_init();
  fonts_init();
  ui_init();
}

void loop() {
  M5.update();
  wifi_update();
  lv_timer_handler();
  delay(5);
}
