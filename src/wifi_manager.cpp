#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>

// Broches SDIO vers l'ESP32-C6 (WiFi via ESP-Hosted) : validées au test T1-07.
static const int C6_SDIO_CLK = 12;
static const int C6_SDIO_CMD = 13;
static const int C6_SDIO_D0 = 11;
static const int C6_SDIO_D1 = 10;
static const int C6_SDIO_D2 = 9;
static const int C6_SDIO_D3 = 8;
static const int C6_RESET = 15;

static const uint32_t CONNECT_TIMEOUT_MS = 20000;

static Preferences g_prefs;
static WifiState g_state = WifiState::Idle;
static uint32_t g_connect_started = 0;
static bool g_pending_save = false;
static String g_pending_ssid;
static String g_pending_pass;
static bool g_scanning = false;

void wifi_begin() {
  WiFi.setPins(C6_SDIO_CLK, C6_SDIO_CMD, C6_SDIO_D0, C6_SDIO_D1, C6_SDIO_D2, C6_SDIO_D3, C6_RESET);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  if (wifi_has_saved()) {
    g_prefs.begin("wifi", true);
    String ssid = g_prefs.getString("ssid", "");
    String pass = g_prefs.getString("pass", "");
    g_prefs.end();
    Serial.printf("WiFi : reconnexion au reseau enregistre (%s)\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
    g_state = WifiState::Connecting;
    g_connect_started = millis();
    g_pending_save = false;
  }
}

bool wifi_has_saved() {
  g_prefs.begin("wifi", true);
  bool ok = g_prefs.getString("ssid", "").length() > 0;
  g_prefs.end();
  return ok;
}

WifiState wifi_state() {
  return g_state;
}

String wifi_ip() {
  return WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("");
}

String wifi_current_ssid() {
  return WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("");
}

void wifi_connect(const String &ssid, const String &pass) {
  Serial.printf("WiFi : connexion a %s\n", ssid.c_str());
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  g_state = WifiState::Connecting;
  g_connect_started = millis();
  g_pending_save = true;
  g_pending_ssid = ssid;
  g_pending_pass = pass;
}

void wifi_forget() {
  g_prefs.begin("wifi", false);
  g_prefs.clear();
  g_prefs.end();
  WiFi.disconnect(true);
  g_state = WifiState::Idle;
}

void wifi_update() {
  if (g_state == WifiState::Connecting) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      g_state = WifiState::Connected;
      Serial.printf("WiFi : connecte, adresse %s\n", WiFi.localIP().toString().c_str());
      if (g_pending_save) {
        g_prefs.begin("wifi", false);
        g_prefs.putString("ssid", g_pending_ssid);
        g_prefs.putString("pass", g_pending_pass);
        g_prefs.end();
        g_pending_save = false;
        g_pending_pass = "";
      }
    } else if (millis() - g_connect_started > CONNECT_TIMEOUT_MS) {
      g_state = WifiState::Failed;
      g_pending_save = false;
      g_pending_pass = "";
      Serial.println("WiFi : echec de connexion (delai depasse)");
    }
  } else if (g_state == WifiState::Connected && WiFi.status() != WL_CONNECTED) {
    // Perte de connexion : le rattachement automatique est actif, on repasse en attente.
    g_state = WifiState::Connecting;
    g_connect_started = millis();
  }
}

void wifi_scan_start() {
  WiFi.scanDelete();
  WiFi.scanNetworks(true);
  g_scanning = true;
}

bool wifi_scan_poll(std::vector<WifiNet> &out) {
  if (!g_scanning) return true;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;
  g_scanning = false;
  out.clear();
  if (n > 0) {
    for (int i = 0; i < n && i < 12; i++) {
      WifiNet net;
      net.ssid = WiFi.SSID(i);
      net.rssi = WiFi.RSSI(i);
      net.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      if (net.ssid.length() == 0) continue;
      out.push_back(net);
    }
  }
  WiFi.scanDelete();
  return true;
}
