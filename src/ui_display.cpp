// Réglages > Affichage : luminosité de l'écran et atténuation automatique après inactivité.
#include <M5Unified.h>
#include <Preferences.h>
#include <lvgl.h>
#include "fonts.h"
#include "ui_common.h"

lv_obj_t *scr_display;

static int g_bri = 70;       // luminosité en % (10 à 100)
static int g_dim_idx = 0;    // 0 = jamais, 1 = 1 min, 2 = 5 min, 3 = 15 min
static bool g_dimmed = false;
static lv_obj_t *d_bri_label, *d_dim_label;
static const int DIM_MIN[] = {0, 1, 5, 15};
static const char *DIM_TXT[] = {"Jamais", "après 1 min", "après 5 min", "après 15 min"};
static const int DIM_LEVEL = 5;  // luminosité (%) pendant l'atténuation

static void apply_brightness(int pct) { M5.Display.setBrightness((uint8_t)(pct * 255 / 100)); }

static void save() {
  Preferences pr;
  pr.begin("display", false);
  pr.putInt("bri", g_bri);
  pr.putInt("dim", g_dim_idx);
  pr.end();
}

static void refresh() {
  char buf[40];
  snprintf(buf, sizeof(buf), "Luminosité : %d %%", g_bri);
  lv_label_set_text(d_bri_label, buf);
  snprintf(buf, sizeof(buf), "Atténuation : %s", DIM_TXT[g_dim_idx]);
  lv_label_set_text(d_dim_label, buf);
}

static void ev_bri_minus(lv_event_t *) {
  g_bri = g_bri > 20 ? g_bri - 10 : 10;
  apply_brightness(g_bri);
  save();
  refresh();
}
static void ev_bri_plus(lv_event_t *) {
  g_bri = g_bri < 90 ? g_bri + 10 : 100;
  apply_brightness(g_bri);
  save();
  refresh();
}
static void ev_dim_next(lv_event_t *) {
  g_dim_idx = (g_dim_idx + 1) % 4;
  save();
  refresh();
}

// Appelé chaque seconde : atténue l'écran après l'inactivité choisie, le rallume au toucher.
static void dim_tick(lv_timer_t *) {
  uint32_t idle = lv_display_get_inactive_time(nullptr);
  if (g_dimmed) {
    if (idle < 1500) {
      g_dimmed = false;
      apply_brightness(g_bri);
    }
    return;
  }
  if (g_dim_idx > 0 && idle > (uint32_t)DIM_MIN[g_dim_idx] * 60000UL) {
    g_dimmed = true;
    apply_brightness(DIM_LEVEL);
  }
}

void display_screen_open() {
  refresh();
  go(scr_display);
}

void ui_display_build() {
  Preferences pr;
  pr.begin("display", true);
  g_bri = pr.getInt("bri", 70);
  g_dim_idx = pr.getInt("dim", 0);
  pr.end();
  if (g_bri < 10 || g_bri > 100) g_bri = 70;
  if (g_dim_idx < 0 || g_dim_idx > 3) g_dim_idx = 0;
  apply_brightness(g_bri);

  Page p = make_page("Affichage", ev_go_settings, NAV_SET);
  scr_display = p.scr;
  lv_obj_t *c = mk_card(p.body, 760, 330, C_LINE);
  lv_obj_set_pos(c, 260, 40);
  d_bri_label = mk_label(c, "", FB20, C_TEXT);
  lv_obj_set_pos(d_bri_label, 28, 40);
  lv_obj_set_pos(mk_button(c, "-", 100, 56, C_CARD2, C_TEXT, FB24, ev_bri_minus), 420, 30);
  lv_obj_set_pos(mk_button(c, "+", 100, 56, C_CARD2, C_TEXT, FB24, ev_bri_plus), 540, 30);
  d_dim_label = mk_label(c, "", FB20, C_TEXT);
  lv_obj_set_pos(d_dim_label, 28, 150);
  lv_obj_set_pos(mk_button(c, "Changer", 200, 56, C_CARD2, C_TEXT, F18, ev_dim_next), 420, 140);
  lv_obj_set_pos(mk_label(c, "L'écran s'atténue après l'inactivité choisie ; un toucher le rallume.", F16, C_MUTED), 28, 250);
  lv_timer_create(dim_tick, 1000, nullptr);
  refresh();
}
