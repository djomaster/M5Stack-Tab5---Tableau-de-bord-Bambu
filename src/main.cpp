// Étape 1 : test de base du Tab5 - écran, tactile, batterie, WiFi.
// Aucune logique Bambu ici : on valide d'abord le matériel et la chaîne de build.

#include <M5Unified.h>
#include <WiFi.h>
#include "config.h"
#include "version.h"

static void drawStatus(const char *wifi, int touchX, int touchY) {
  auto &d = M5.Display;
  d.startWrite();
  d.fillRect(0, 80, d.width(), 260, TFT_BLACK);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setTextSize(2);
  d.setCursor(20, 90);
  d.printf("Ecran : %d x %d", d.width(), d.height());
  d.setCursor(20, 130);
  d.printf("Batterie : %d %%", (int)M5.Power.getBatteryLevel());
  d.setCursor(20, 170);
  d.printf("WiFi : %s", wifi);
  d.setCursor(20, 210);
  d.printf("PSRAM libre : %u Mo", (unsigned)(ESP.getFreePsram() / (1024 * 1024)));
  d.setCursor(20, 250);
  if (touchX >= 0) d.printf("Toucher : %d, %d", touchX, touchY);
  else d.print("Toucher : -");
  d.endWrite();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  M5.Display.setBrightness(180);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(20, 20);
  M5.Display.printf("%s - v%s", FW_NAME, FW_VERSION);
  Serial.printf("%s v%s\n", FW_NAME, FW_VERSION);

  // WiFi : le P4 n'a pas de radio, il passe par l'ESP32-C6 en SDIO.
  WiFi.setPins(C6_SDIO_CLK, C6_SDIO_CMD, C6_SDIO_D0, C6_SDIO_D1, C6_SDIO_D2, C6_SDIO_D3, C6_RESET);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  M5.update();

  int tx = -1, ty = -1;
  if (M5.Touch.getCount() > 0) {
    auto t = M5.Touch.getDetail(0);
    tx = t.x;
    ty = t.y;
  }

  static uint32_t last = 0;
  if (millis() - last > 300) {
    last = millis();
    String wifi = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "connexion...";
    drawStatus(wifi.c_str(), tx, ty);
  }
}
