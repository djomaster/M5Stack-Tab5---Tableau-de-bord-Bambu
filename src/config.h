#pragma once
// Copiez ce fichier en config.h (ignoré par git) et renseignez vos valeurs.

#define WIFI_SSID "votre-ssid"
#define WIFI_PASS "votre-mot-de-passe"

// Broches SDIO vers l'ESP32-C6 (WiFi via ESP-Hosted) : CLK, CMD, D0, D1, D2, D3, RESET.
// Valeurs issues de la carte des broches du Tab5 : à valider au premier test.
#define C6_SDIO_CLK 12
#define C6_SDIO_CMD 13
#define C6_SDIO_D0 11
#define C6_SDIO_D1 10
#define C6_SDIO_D2 9
#define C6_SDIO_D3 8
#define C6_RESET 15
