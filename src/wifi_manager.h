#pragma once
#include <Arduino.h>
#include <vector>

enum class WifiState { Idle, Connecting, Connected, Failed };

struct WifiNet {
  String ssid;
  int rssi;
  bool secured;
};

// Démarre la radio (ESP32-C6 en SDIO) et se reconnecte au réseau enregistré, s'il y en a un.
void wifi_begin();
// À appeler à chaque tour de loop().
void wifi_update();

bool wifi_has_saved();
WifiState wifi_state();
String wifi_ip();
String wifi_current_ssid();

// Connexion à un réseau ; il est enregistré dans la mémoire de l'appareil une fois connecté.
void wifi_connect(const String &ssid, const String &pass);
// Oublie le réseau enregistré.
void wifi_forget();

// Recherche des réseaux : wifi_scan_start(), puis wifi_scan_poll() jusqu'à ce qu'il renvoie true.
void wifi_scan_start();
bool wifi_scan_poll(std::vector<WifiNet> &out);
