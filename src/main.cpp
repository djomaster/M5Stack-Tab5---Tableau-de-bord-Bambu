// Tableau de bord Bambu Lab - M5Stack Tab5
// Version 0.2.0 : interface LVGL (accueil avec données d'exemple, réglages, configuration WiFi à l'écran).
// Pas encore de connexion aux imprimantes (étape suivante).

#include <M5Unified.h>
#include <lvgl.h>
#include "version.h"
#include "display_port.h"
#include "fonts.h"
#include "ui.h"
#include "wifi_manager.h"

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.printf("%s v%s\n", FW_NAME, FW_VERSION);

  wifi_begin();
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
