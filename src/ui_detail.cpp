// Vue détaillée d'une imprimante (progression, températures, bobines de l'AMS) et réglage des alertes sonores.
#include <lvgl.h>
#include "alerts.h"
#include "bambu.h"
#include "camera.h"
#include "fonts.h"
#include "printers.h"
#include "tiger.h"
#include "diag.h"
#include "ui_common.h"

// =====================================================================
//  Vue détaillée
// =====================================================================
static int d_idx = 0;
static lv_obj_t *d_name, *d_status, *d_pct, *d_bar, *d_file, *d_layer, *d_remain, *d_nozzle, *d_bed, *d_ams;
static String d_ams_sig;

static void ev_detail_back(lv_event_t *e) { ev_go_home(e); }

// Poids restant d'après l'inventaire Tiger Studio : par puce RFID (sûr), sinon par couleur et matière (approximatif).
static int inventory_grams(const Tray &T, bool &approx) {
  approx = false;
  if (!T.present || !tiger_signed_in()) return -1;
  int i = -1;
  bool zero = true;
  for (size_t k = 0; k < T.tag.length(); k++)
    if (T.tag[k] != '0') zero = false;
  if (!zero) i = tiger_find_tag(T.tag);
  if (i < 0) {
    i = tiger_find_color(T.color, T.type);
    approx = (i >= 0);
  }
  Spool sp;
  if (i < 0 || !tiger_get(i, sp)) return -1;
  return sp.weight;
}

static void tray_card(lv_obj_t *parent, const Tray &T, bool active, int x, int y, bool show_pct) {
  lv_obj_t *c = mk_card(parent, 150, 84, active ? C_ACC : C_LINE2);
  lv_obj_set_pos(c, x, y);
  if (T.present) {
    lv_obj_t *sw = lv_obj_create(c);
    plain(sw, T.color);
    lv_obj_set_size(sw, 34, 34);
    lv_obj_set_style_radius(sw, 17, 0);
    lv_obj_set_style_border_width(sw, 2, 0);
    lv_obj_set_style_border_color(sw, col(C_LINE2), 0);
    lv_obj_set_pos(sw, 12, 10);
    lv_obj_set_pos(mk_label(c, T.type.c_str(), FB16, C_TEXT), 56, 14);
    // Une seule valeur en bas de carte : le poids de l'inventaire Tiger s'il est connu, sinon le pourcentage de l'imprimante.
    // Pas de pourcentage pour une bobine externe, ni quand l'imprimante ne le connaît pas (bobine sans puce).
    bool shown = false;
    if (show_pct) {
      bool approx;
      int g = inventory_grams(T, approx);
      if (g >= 0) {
        String gs = String(approx ? "env. " : "") + String(g) + " g";
        lv_obj_set_pos(mk_label(c, gs.c_str(), F16, approx ? C_MUTED : C_ACC_T), 12, 52);
        shown = true;
      }
    }
    if (!shown && show_pct && T.remain >= 0) {
      String r = String(T.remain) + " %";
      lv_obj_set_pos(mk_label(c, r.c_str(), F16, C_MUTED), 12, 52);
    }
  } else {
    lv_obj_set_pos(mk_label(c, "Vide", F16, C_MUTED), 12, 28);
  }
}

static void rebuild_ams(const PrinterState &s) {
  diag_mark("ui:rebuild_ams");
  lv_obj_clean(d_ams);
  lv_obj_set_pos(mk_label(d_ams, "Filaments", FB20, C_TEXT), 24, 18);
  int units = 0;
  for (int u = 0; u < 4; u++)
    if (s.unit_id[u] >= 0) units = u + 1;
  for (int u = 0; u < units; u++) {
    bool ht = s.unit_ht[u];
    String t = "AMS " + String(u + 1) + (ht ? " HT" : "");
    lv_obj_set_pos(mk_label(d_ams, t.c_str(), F16, C_MUTED), 24, 62 + u * 112);
    int slots = ht ? 1 : 4;  // un AMS HT n'a qu'une bobine
    for (int k = 0; k < slots; k++) {
      bool active = ht ? (s.tray_now == s.unit_id[u]) : (s.tray_now == u * 4 + k);
      tray_card(d_ams, s.trays[u * 4 + k], active, 24 + k * 164, 86 + u * 112, true);
    }
  }
  // Bobine(s) externe(s)
  int y = 62 + units * 112;
  lv_obj_set_pos(mk_label(d_ams, "Bobine externe", F16, C_MUTED), 24, y);
  int n_ext = s.ext[1].present ? 2 : 1;
  for (int k = 0; k < n_ext; k++) tray_card(d_ams, s.ext[k], s.tray_now == 254 && k == 0, 24 + k * 164, y + 24, false);
  if (units == 0 && !s.ext[0].present && !s.ext[1].present)
    lv_obj_set_pos(mk_label(d_ams, "Aucun AMS détecté.", F16, C_MUTED), 24, y + 122);
}

static void detail_tick(lv_timer_t *) {
  if (lv_screen_active() != scr_detail) return;
  PrinterState s;
  if (d_idx >= printers_count() || !bambu_state(d_idx, s)) return;
  bool live = s.link == Link::Connected && s.got_report;
  bool running = live && s.gcode == "RUNNING";
  bool problem = s.link == Link::AuthFailed || s.link == Link::Unreachable || (live && (s.error != 0 || s.gcode == "FAILED"));
  lv_label_set_text(d_name, printers_get(d_idx).name.c_str());
  lv_label_set_text(d_status, link_text((int)s.link, s.gcode.c_str(), s.got_report, s.error));
  lv_obj_set_style_text_color(d_status, col(problem ? C_WARN_T : (running ? C_ACC_T : C_MUTED)), 0);
  char buf[96];
  if (live) {
    snprintf(buf, sizeof(buf), "%d %%", s.percent);
    lv_label_set_text(d_pct, buf);
    lv_bar_set_value(d_bar, s.percent, LV_ANIM_OFF);
    lv_label_set_text(d_file, s.file.c_str());
    snprintf(buf, sizeof(buf), "Couche %d / %d", s.layer, s.total_layers);
    lv_label_set_text(d_layer, buf);
    String r = "Temps restant : " + fmt_remaining(s.remaining_min);
    lv_label_set_text(d_remain, r.c_str());
    snprintf(buf, sizeof(buf), "Buse : %.0f °C", s.nozzle);
    lv_label_set_text(d_nozzle, buf);
    snprintf(buf, sizeof(buf), "Plateau : %.0f °C", s.bed);
    lv_label_set_text(d_bed, buf);
  } else {
    lv_label_set_text(d_pct, "-");
    lv_bar_set_value(d_bar, 0, LV_ANIM_OFF);
    lv_label_set_text(d_file, "");
    lv_label_set_text(d_layer, "");
    lv_label_set_text(d_remain, "");
    lv_label_set_text(d_nozzle, "");
    lv_label_set_text(d_bed, "");
  }
  // Les bobines ne sont redessinées que si elles ont changé.
  String sig = String(tiger_data_revision()) + "/" + String(s.tray_now) + ":";
  for (int u = 0; u < 4; u++) sig += String(s.unit_id[u]) + (s.unit_ht[u] ? "H" : "N");
  for (int i = 0; i < MAX_TRAYS; i++)
    sig += String(s.trays[i].present) + s.trays[i].tag + s.trays[i].type + String(s.trays[i].color, HEX) + String(s.trays[i].remain) + ";";
  for (int k = 0; k < 2; k++)
    sig += String(s.ext[k].present) + s.ext[k].type + String(s.ext[k].color, HEX) + String(s.ext[k].remain) + ";";
  if (sig != d_ams_sig) {
    d_ams_sig = sig;
    rebuild_ams(s);
  }
}

// Sonde caméra (étape 6) : fenêtre de résultats au-dessus de l'écran.
static lv_obj_t *cam_ov = nullptr, *cam_txt = nullptr;
static lv_timer_t *cam_tm = nullptr;
static String cam_last;

static void cam_refresh(lv_timer_t *) {
  String r = camera_probe_report();
  if (r != cam_last && cam_txt) {
    cam_last = r;
    lv_label_set_text(cam_txt, r.c_str());
  }
}

static void ev_cam_close(lv_event_t *) {
  if (cam_tm) lv_timer_delete(cam_tm);
  cam_tm = nullptr;
  if (cam_ov) lv_obj_delete(cam_ov);
  cam_ov = nullptr;
  cam_txt = nullptr;
}

static void ev_cam_open(lv_event_t *) {
  if (cam_ov || camera_probe_running()) return;
  camera_probe_start(printers_get(d_idx));
  cam_last = "";
  cam_ov = lv_obj_create(lv_layer_top());
  lv_obj_set_size(cam_ov, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(cam_ov, col(C_BG), 0);
  lv_obj_set_style_bg_opa(cam_ov, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cam_ov, 0, 0);
  lv_obj_set_style_radius(cam_ov, 0, 0);
  lv_obj_clear_flag(cam_ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *t = mk_label(cam_ov, "Test caméra (diagnostic du flux)", FB28, C_TEXT);
  lv_obj_set_pos(t, 30, 20);
  cam_txt = mk_label(cam_ov, "Démarrage...", F18, C_TEXT);
  lv_obj_set_width(cam_txt, 1200);
  lv_label_set_long_mode(cam_txt, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(cam_txt, 30, 80);
  lv_obj_t *b = mk_button(cam_ov, "Fermer", 200, 60, C_CARD2, C_TEXT, F20, ev_cam_close, nullptr);
  lv_obj_set_pos(b, 1040, 630);
  cam_tm = lv_timer_create(cam_refresh, 500, nullptr);
}

void detail_open(int printer_index) {
  d_idx = printer_index;
  d_ams_sig = "";
  go(scr_detail);
  detail_tick(nullptr);
}

// =====================================================================
//  Alertes
// =====================================================================
static lv_obj_t *al_state_label, *al_vol_label;

static void refresh_alerts() {
  lv_label_set_text(al_state_label, alerts_enabled() ? "Sons : activés" : "Sons : désactivés");
  char buf[32];
  snprintf(buf, sizeof(buf), "Volume : %d / 5", alerts_volume());
  lv_label_set_text(al_vol_label, buf);
}

static void ev_toggle(lv_event_t *) {
  alerts_set_enabled(!alerts_enabled());
  refresh_alerts();
}
static void ev_vol_minus(lv_event_t *) {
  alerts_set_volume(alerts_volume() - 1);
  refresh_alerts();
  alerts_play(Sound::Test);
}
static void ev_vol_plus(lv_event_t *) {
  alerts_set_volume(alerts_volume() + 1);
  refresh_alerts();
  alerts_play(Sound::Test);
}
static void test_sound(Sound s) {
  if (!alerts_enabled()) {
    toast("Les sons sont désactivés");
    return;
  }
  alerts_play(s);
}
static void ev_test_finish(lv_event_t *) { test_sound(Sound::Finish); }
static void ev_test_error(lv_event_t *) { test_sound(Sound::Error); }

void alerts_screen_open() {
  refresh_alerts();
  go(scr_alerts);
}

static void build_alerts() {
  Page p = make_page("Alertes", ev_go_settings, NAV_SET);
  scr_alerts = p.scr;
  lv_obj_t *c = mk_card(p.body, 760, 440, C_LINE);
  lv_obj_set_pos(c, 260, 40);
  lv_obj_set_pos(mk_label(c, "Un son est émis à la fin d'une impression et quand une erreur apparaît.", F16, C_MUTED), 28, 20);
  al_state_label = mk_label(c, "", FB20, C_TEXT);
  lv_obj_set_pos(al_state_label, 28, 76);
  lv_obj_set_pos(mk_button(c, "Activer / désactiver", 300, 56, C_CARD2, C_TEXT, F18, ev_toggle), 420, 66);
  al_vol_label = mk_label(c, "", FB20, C_TEXT);
  lv_obj_set_pos(al_vol_label, 28, 170);
  lv_obj_set_pos(mk_button(c, "-", 100, 56, C_CARD2, C_TEXT, FB24, ev_vol_minus), 420, 160);
  lv_obj_set_pos(mk_button(c, "+", 100, 56, C_CARD2, C_TEXT, FB24, ev_vol_plus), 540, 160);
  lv_obj_set_pos(mk_button(c, "Tester : fin d'impression", 330, 60, C_CARD2, C_TEXT, F18, ev_test_finish), 28, 280);
  lv_obj_set_pos(mk_button(c, "Tester : erreur", 330, 60, C_CARD2, C_TEXT, F18, ev_test_error), 380, 280);
}

// =====================================================================
void ui_detail_build() {
  Page p = make_page("Détail de l'imprimante", ev_detail_back, NAV_HOME);
  scr_detail = p.scr;

  lv_obj_t *left = mk_card(p.body, 540, 520, C_LINE2);
  lv_obj_set_pos(left, 24, 24);
  d_name = mk_label(left, "", FB28, C_TEXT);
  lv_obj_set_width(d_name, 330);
  lv_label_set_long_mode(d_name, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(d_name, 24, 20);
  lv_obj_set_pos(mk_button(left, "Caméra", 130, 48, C_CARD2, C_TEXT, F18, ev_cam_open, nullptr), 380, 16);
  d_status = mk_label(left, "", F20, C_MUTED);
  lv_obj_set_pos(d_status, 24, 70);
  d_pct = mk_label(left, "-", FB28, C_TEXT);
  lv_obj_set_pos(d_pct, 24, 124);
  d_bar = lv_bar_create(left);
  lv_obj_set_size(d_bar, 490, 22);
  lv_obj_set_pos(d_bar, 24, 184);
  lv_obj_set_style_bg_color(d_bar, col(C_LINE), 0);
  lv_obj_set_style_bg_color(d_bar, col(C_ACC), LV_PART_INDICATOR);
  d_file = mk_label(left, "", F20, C_TEXT);
  lv_obj_set_width(d_file, 490);
  lv_label_set_long_mode(d_file, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(d_file, 24, 232);
  d_layer = mk_label(left, "", F20, C_MUTED);
  lv_obj_set_pos(d_layer, 24, 290);
  d_remain = mk_label(left, "", F20, C_MUTED);
  lv_obj_set_pos(d_remain, 24, 336);
  d_nozzle = mk_label(left, "", F20, C_MUTED);
  lv_obj_set_pos(d_nozzle, 24, 396);
  d_bed = mk_label(left, "", F20, C_MUTED);
  lv_obj_set_pos(d_bed, 24, 442);

  d_ams = mk_card(p.body, 700, 520, C_LINE2);
  lv_obj_set_pos(d_ams, 580, 24);
  lv_obj_set_style_pad_all(d_ams, 0, 0);

  build_alerts();
  lv_timer_create(detail_tick, 1000, nullptr);
}
