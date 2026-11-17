#pragma once
#include <Arduino.h>

static const int MAX_PRINTERS = 3;

struct PrinterCfg {
  String name;
  String ip;
  String serial;
  String code;  // code d'accès LAN : stocké uniquement dans la mémoire de l'appareil
};

void printers_load();
int printers_count();
const PrinterCfg &printers_get(int i);
bool printers_add(const PrinterCfg &p);  // false si le maximum est atteint ou si le numéro de série existe déjà
void printers_remove(int i);
int printers_revision();  // augmente à chaque ajout ou suppression
