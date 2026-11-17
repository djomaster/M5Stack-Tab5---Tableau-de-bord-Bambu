#include "discovery.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <vector>

static WiFiUDP g_udp;
static bool g_on = false;
static uint32_t g_last_search = 0;
static std::vector<Found> g_found;

static String header(const String &text, const char *name) {
  String low = text;
  low.toLowerCase();
  String key = String(name);
  key.toLowerCase();
  key += ":";
  int p = low.indexOf(key);
  if (p < 0) return "";
  p += key.length();
  int e = text.indexOf('\n', p);
  String v = text.substring(p, e < 0 ? text.length() : e);
  v.trim();
  return v;
}

static String model_name(const String &code) {
  if (code == "BL-P001") return "X1 Carbon";
  if (code == "BL-P002") return "X1";
  if (code == "C11") return "P1P";
  if (code == "C12") return "P1S";
  if (code == "N1") return "A1 mini";
  if (code == "N2S") return "A1";
  if (code == "O1D") return "H2D";
  if (code == "O1S") return "H2S";
  return code.length() ? "Modèle " + code : "Bambu Lab";
}

static void send_search() {
  const char *msg =
      "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1990\r\nMAN: \"ssdp:discover\"\r\nMX: 3\r\n"
      "ST: urn:bambulab-com:device:3dprinter:1\r\n\r\n";
  g_udp.beginPacket(IPAddress(239, 255, 255, 250), 1990);
  g_udp.write((const uint8_t *)msg, strlen(msg));
  g_udp.endPacket();
}

void discovery_start() {
  if (g_on) return;
  g_found.clear();
  if (g_udp.beginMulticast(IPAddress(239, 255, 255, 250), 1990)) {
    g_on = true;
    send_search();
    g_last_search = millis();
    Serial.println("Recherche : ecoute SSDP demarree");
  } else {
    Serial.println("Recherche : impossible d'ecouter le port 1990");
  }
}

void discovery_stop() {
  if (!g_on) return;
  g_udp.stop();
  g_on = false;
}

void discovery_poll() {
  if (!g_on) return;
  if (millis() - g_last_search > 6000) {
    send_search();
    g_last_search = millis();
  }
  char buf[1024];
  while (g_udp.parsePacket() > 0) {
    int n = g_udp.read((uint8_t *)buf, sizeof(buf) - 1);
    if (n <= 0) break;
    buf[n] = 0;
    String text(buf);
    String low = text;
    low.toLowerCase();
    if (low.indexOf("bambulab") < 0) continue;
    Found f;
    f.serial = header(text, "USN");
    String loc = header(text, "Location");
    f.ip = loc.length() ? loc : g_udp.remoteIP().toString();
    f.model = model_name(header(text, "DevModel.bambu.com"));
    if (f.serial.length() == 0) continue;
    bool known = false;
    for (auto &x : g_found)
      if (x.serial == f.serial) known = true;
    if (!known) {
      g_found.push_back(f);
      Serial.printf("Recherche : %s %s (%s)\n", f.model.c_str(), f.ip.c_str(), f.serial.c_str());
    }
  }
}

int discovery_count() { return (int)g_found.size(); }
Found discovery_get(int i) { return g_found[i]; }
