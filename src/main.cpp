// Tableau de bord Bambu Lab - M5Stack Tab5
// Version 0.5.0 : inventaire Tiger Studio et rapprochement avec les bobines de l'AMS.

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "alerts.h"
#include "bambu.h"
#include "display_port.h"
#include "diag.h"
#include "fonts.h"
#include <esp_system.h>
#include "printers.h"
#include "tiger.h"
#include "ui.h"
#include "version.h"
#include "wifi_manager.h"

static void log_memory(const char *where) {
  Serial.printf("Mem [%s] : interne libre %u (bloc max %u) | PSRAM libre %u\n", where,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  Serial.printf("%s v%s\n", FW_NAME, FW_VERSION);
  Serial.printf("Demarrage : cause %d (1 mise sous tension, 3 logiciel, 4 panne, 5 interruption watchdog, 6 watchdog tache, 7 watchdog)\n", (int)esp_reset_reason());

  printers_load();
  alerts_load();
  wifi_begin();
  bambu_start();
  tiger_begin();  // crée le verrou et la tâche réseau avant que l'interface ne les interroge
  log_memory("avant interface");
  // Les objets LVGL sont alloues en PSRAM par src/lv_mem_psram.cpp (memoire interne reservee au WiFi/TLS).
  display_port_init();
  log_memory("apres ecran");
  fonts_init();
  log_memory("apres polices");
  ui_init();
  log_memory("demarrage");
}

void loop() {
  diag_loop();
  static uint32_t last_mem = 0;
  if (millis() - last_mem > 10000) {
    last_mem = millis();
    log_memory("10 s");
  }
  M5.update();
  wifi_update();
  lv_timer_handler();
  delay(5);
}
