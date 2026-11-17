#include "printers.h"
#include <Preferences.h>

static PrinterCfg g_list[MAX_PRINTERS];
static int g_count = 0;
static int g_rev = 0;

static void save_all() {
  Preferences pr;
  pr.begin("printers", false);
  pr.clear();
  pr.putInt("n", g_count);
  for (int i = 0; i < g_count; i++) {
    String k = "p" + String(i);
    pr.putString((k + "name").c_str(), g_list[i].name);
    pr.putString((k + "ip").c_str(), g_list[i].ip);
    pr.putString((k + "sn").c_str(), g_list[i].serial);
    pr.putString((k + "code").c_str(), g_list[i].code);
  }
  pr.end();
}

void printers_load() {
  Preferences pr;
  pr.begin("printers", true);
  g_count = pr.getInt("n", 0);
  if (g_count > MAX_PRINTERS) g_count = MAX_PRINTERS;
  if (g_count < 0) g_count = 0;
  for (int i = 0; i < g_count; i++) {
    String k = "p" + String(i);
    g_list[i].name = pr.getString((k + "name").c_str(), "");
    g_list[i].ip = pr.getString((k + "ip").c_str(), "");
    g_list[i].serial = pr.getString((k + "sn").c_str(), "");
    g_list[i].code = pr.getString((k + "code").c_str(), "");
  }
  pr.end();
  g_rev++;
  Serial.printf("Imprimantes : %d enregistree(s)\n", g_count);
}

int printers_count() { return g_count; }
const PrinterCfg &printers_get(int i) { return g_list[i]; }
int printers_revision() { return g_rev; }

bool printers_add(const PrinterCfg &p) {
  if (g_count >= MAX_PRINTERS) return false;
  for (int i = 0; i < g_count; i++)
    if (g_list[i].serial.equalsIgnoreCase(p.serial)) return false;
  g_list[g_count++] = p;
  save_all();
  g_rev++;
  return true;
}

void printers_remove(int i) {
  if (i < 0 || i >= g_count) return;
  for (int j = i; j < g_count - 1; j++) g_list[j] = g_list[j + 1];
  g_count--;
  g_list[g_count] = PrinterCfg();
  save_all();
  g_rev++;
}
