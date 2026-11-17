#include "alerts.h"
#include <M5Unified.h>
#include <Preferences.h>
#include <lvgl.h>
#include "bambu.h"
#include "printers.h"

static bool g_on = true;
static int g_vol = 3;

struct Note {
  int freq;  // Hz, 0 = silence
  int ms;
};

static const Note MEL_FINISH[] = {{880, 150}, {1175, 150}, {1568, 300}, {0, 0}};
static const Note MEL_ERROR[] = {{660, 250}, {0, 120}, {660, 250}, {0, 120}, {660, 250}, {0, 0}};

static const Note *g_seq = nullptr;
static int g_idx = 0;
static uint32_t g_next = 0;
static lv_timer_t *g_timer = nullptr;

static void apply_volume() { M5.Speaker.setVolume(g_vol * 51); }

static void seq_step(lv_timer_t *) {
  if (!g_seq || millis() < g_next) return;
  const Note &n = g_seq[g_idx];
  if (n.ms == 0) {
    g_seq = nullptr;
    return;
  }
  if (n.freq > 0) M5.Speaker.tone(n.freq, n.ms);
  g_next = millis() + n.ms + 40;
  g_idx++;
}

void alerts_load() {
  Preferences pr;
  pr.begin("alerts", true);
  g_on = pr.getBool("on", true);
  g_vol = pr.getInt("vol", 3);
  pr.end();
  if (g_vol < 1) g_vol = 1;
  if (g_vol > 5) g_vol = 5;
  apply_volume();
}

static void save() {
  Preferences pr;
  pr.begin("alerts", false);
  pr.putBool("on", g_on);
  pr.putInt("vol", g_vol);
  pr.end();
}

bool alerts_enabled() { return g_on; }
int alerts_volume() { return g_vol; }

void alerts_set_enabled(bool on) {
  g_on = on;
  save();
}

void alerts_set_volume(int v) {
  g_vol = v < 1 ? 1 : (v > 5 ? 5 : v);
  apply_volume();
  save();
}

void alerts_play(Sound s) {
  if (!g_timer) g_timer = lv_timer_create(seq_step, 30, nullptr);
  apply_volume();
  g_seq = (s == Sound::Error) ? MEL_ERROR : MEL_FINISH;
  g_idx = 0;
  g_next = 0;
}

// Détection des changements d'état : un son à la fin d'une impression, un autre à l'apparition d'une erreur.
// Le premier rapport reçu après la connexion sert de référence (aucun son).
void alerts_tick() {
  static String prev_state[MAX_PRINTERS];
  static int prev_error[MAX_PRINTERS];
  static bool seen[MAX_PRINTERS];
  static int rev = -1;
  if (rev != printers_revision()) {
    rev = printers_revision();
    for (int i = 0; i < MAX_PRINTERS; i++) seen[i] = false;
  }
  for (int i = 0; i < printers_count(); i++) {
    PrinterState s;
    if (!bambu_state(i, s) || !s.got_report) {
      seen[i] = false;
      continue;
    }
    if (seen[i] && g_on) {
      if (prev_state[i] == "RUNNING" && s.gcode == "FINISH") alerts_play(Sound::Finish);
      else if ((s.gcode == "FAILED" && prev_state[i] != "FAILED") || (s.error != 0 && prev_error[i] == 0))
        alerts_play(Sound::Error);
    }
    prev_state[i] = s.gcode;
    prev_error[i] = s.error;
    seen[i] = true;
  }
}
