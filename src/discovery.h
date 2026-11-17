#pragma once
#include <Arduino.h>

struct Found {
  String ip;
  String serial;
  String model;  // nom lisible ou code du modèle
};

// Recherche des imprimantes Bambu sur le réseau local (annonces SSDP, port 1990).
void discovery_start();
void discovery_stop();
void discovery_poll();  // à appeler régulièrement tant que la recherche est active
int discovery_count();
Found discovery_get(int i);
