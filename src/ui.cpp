#include "diag.h"
#include "ui.h"
#include <M5Unified.h>
#include <lvgl.h>
#include <vector>
#include "alerts.h"
#include "bambu.h"
#include "fonts.h"
#include "printers.h"
#include "ui_common.h"
#include "version.h"
#include "wifi_manager.h"

static void ev_soon(lv_event_t *) { toast("Disponible dans une prochaine version"); }
static void ev_go_about(lv_event_t *) { go(scr_about); }
static void ev_go_printers(lv_event_t *) { printers_screen_open(); }
static void ev_go_wizard(lv_event_t *) { wizard_open(); }
static void ev_go_alerts(lv_event_t *) { alerts_screen_open(); }
static void ev_go_display(lv_event_t *) { display_screen_open(); }
static void ev_go_tiger(lv_event_t *) { tiger_screen_open(); }
static void ev_tile(lv_event_t *e) { detail_open((int)(intptr_t)lv_event_get_user_data(e)); }
static void ev_go_wifi(lv_event_t *);

// ---------- Accueil : une tuile par imprimante ----------
static Page home_page;
struct Tile {
  lv_obj_t *card, *status, *pct, *bar, *file, *layer, *remain, *temps;
};
static Tile tiles[MAX_PRINTERS];
static int built_rev = -1;
static int built_count = 0;

static void home_rebuild() {
  lv_obj_clean(home_page.body);
  int n = printers_count();
  built_count = n;
  built_rev = printers_revision();
  for (int i = 0; i < n; i++) {
    Tile &t = tiles[i];
    int x = 24 + i * 416;
    t.card = mk_card(home_page.body, 400, 520, C_LINE2);
    lv_obj_set_pos(t.card, x, 24);
    lv_obj_t *name = mk_label(t.card, printers_get(i).name.c_str(), FB24, C_TEXT);
    lv_obj_set_width(name, 352);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(name, 24, 20);
    t.status = mk_label(t.card, "", F18, C_MUTED);
    lv_obj_set_pos(t.status, 24, 62);
    t.pct = mk_label(t.card, "-", FB28, C_TEXT);
    lv_obj_set_pos(t.pct, 24, 112);
    t.bar = lv_bar_create(t.card);
    lv_obj_set_size(t.bar, 352, 18);
    lv_obj_set_pos(t.bar, 24, 172);
    lv_obj_set_style_bg_color(t.bar, col(C_LINE), 0);
    lv_obj_set_style_bg_color(t.bar, col(C_ACC), LV_PART_INDICATOR);
    lv_bar_set_value(t.bar, 0, LV_ANIM_OFF);
    t.file = mk_label(t.card, "", F18, C_TEXT);
    lv_obj_set_width(t.file, 352);
    lv_label_set_long_mode(t.file, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(t.file, 24, 216);
    t.layer = mk_label(t.card, "", F18, C_MUTED);
    lv_obj_set_pos(t.layer, 24, 256);
    t.remain = mk_label(t.card, "", F18, C_MUTED);
    lv_obj_set_pos(t.remain, 24, 292);
    t.temps = mk_label(t.card, "", F18, C_MUTED);
    lv_obj_set_pos(t.temps, 24, 328);
    lv_obj_remove_flag(t.bar, LV_OBJ_FLAG_CLICKABLE);
    for (uint32_t k = 0; k < lv_obj_get_child_count(t.card); k++) lv_obj_add_flag(lv_obj_get_child(t.card, k), LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(t.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(t.card, ev_tile, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    lv_obj_t *hint = mk_label(t.card, "Touchez pour le détail", F16, C_LINE2);
    lv_obj_set_pos(hint, 24, 480);
    lv_obj_add_flag(hint, LV_OBJ_FLAG_EVENT_BUBBLE);
  }
  if (n < MAX_PRINTERS) {
    lv_obj_t *c = mk_card(home_page.body, 400, 520, C_LINE2);
    lv_obj_set_pos(c, 24 + n * 416, 24);
    lv_obj_set_style_bg_opa(c, LV_OPA_50, 0);
    lv_obj_t *plus = mk_label(c, "+", FB28, C_MUTED);
    lv_obj_align(plus, LV_ALIGN_CENTER, 0, -30);
    lv_obj_t *txt = mk_label(c, n == 0 ? "Ajouter une imprimante" : "Ajouter une autre imprimante", F20, C_MUTED);
    lv_obj_align(txt, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c, ev_go_wizard, LV_EVENT_CLICKED, nullptr);
  }
}

static void home_update() {
  diag_mark("ui:home_update");
  if (printers_revision() != built_rev) home_rebuild();
  for (int i = 0; i < built_count; i++) {
    PrinterState s;
    Tile &t = tiles[i];
    if (!bambu_state(i, s)) continue;
    bool live = s.link == Link::Connected && s.got_report;
    bool problem = s.link == Link::AuthFailed || s.link == Link::Unreachable || (live && (s.error != 0 || s.gcode == "FAILED"));
    bool running = live && s.gcode == "RUNNING";
    uint32_t border = problem ? C_WARN : (running ? C_ACC : C_LINE2);
    lv_obj_set_style_border_color(t.card, col(border), 0);
    lv_label_set_text(t.status, link_text((int)s.link, s.gcode.c_str(), s.got_report, s.error));
    lv_obj_set_style_text_color(t.status, col(problem ? C_WARN_T : (running ? C_ACC_T : C_MUTED)), 0);
    char buf[96];
    if (live) {
      snprintf(buf, sizeof(buf), "%d %%", s.percent);
      lv_label_set_text(t.pct, buf);
      lv_bar_set_value(t.bar, s.percent, LV_ANIM_OFF);
      lv_label_set_text(t.file, s.file.c_str());
      snprintf(buf, sizeof(buf), "Couche %d / %d", s.layer, s.total_layers);
      lv_label_set_text(t.layer, buf);
      String r = "Temps restant : " + fmt_remaining(s.remaining_min);
      lv_label_set_text(t.remain, r.c_str());
      snprintf(buf, sizeof(buf), "Buse %.0f °C   Plateau %.0f °C", s.nozzle, s.bed);
      lv_label_set_text(t.temps, buf);
      if (s.error != 0) {
        snprintf(buf, sizeof(buf), "Code d'erreur : %d", s.error);
        lv_label_set_text(t.file, buf);
      }
    } else {
      lv_label_set_text(t.pct, "-");
      lv_bar_set_value(t.bar, 0, LV_ANIM_OFF);
      lv_label_set_text(t.file, s.link == Link::AuthFailed ? "Vérifiez le code d'accès LAN" : "");
      lv_label_set_text(t.layer, "");
      lv_label_set_text(t.remain, "");
      lv_label_set_text(t.temps, "");
    }
  }
}

static void build_home() {
  home_page = make_page("Tableau de bord", nullptr, NAV_HOME);
  scr_home = home_page.scr;
  home_rebuild();
}

// ---------- Réglages ----------
static lv_obj_t *s_reboot_btn;
// Redémarrage en deux appuis : le premier demande confirmation (4 s), le second redémarre.
static void ev_reboot(lv_event_t *) {
  static uint32_t first = 0;
  uint32_t now = millis();
  if (first && now - first < 4000) {
    toast("Redémarrage...");
    lv_timer_handler();
    delay(300);
    ESP.restart();
    return;
  }
  first = now;
  lv_label_set_text(lv_obj_get_child(s_reboot_btn, 0), "Appuyez encore pour confirmer le redémarrage");
  lv_timer_t *tm = lv_timer_create([](lv_timer_t *) { lv_label_set_text(lv_obj_get_child(s_reboot_btn, 0), "Redémarrer la tablette"); }, 4000, nullptr);
  lv_timer_set_repeat_count(tm, 1);
}
static void build_settings() {
  Page p = make_page("Réglages", nullptr, NAV_SET);
  scr_settings = p.scr;
  struct Item {
    const char *txt;
    lv_event_cb_t cb;
  };
  Item items[] = {
      {"WiFi", ev_go_wifi}, {"Imprimantes", ev_go_printers}, {"Tiger Studio", ev_go_tiger}, {"Alertes", ev_go_alerts}, {"Affichage", ev_go_display}, {"À propos", ev_go_about},
  };
  int y = 24;
  for (auto &it : items) {
    lv_obj_t *b = mk_button(p.body, it.txt, 900, 62, C_CARD, C_TEXT, FB20, it.cb);
    lv_obj_set_pos(b, 190, y);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, col(C_LINE), 0);
    lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 24, 0);
    y += 74;
  }
  s_reboot_btn = mk_button(p.body, "Redémarrer la tablette", 900, 62, C_CARD, C_WARN_T, FB20, ev_reboot);
  lv_obj_set_pos(s_reboot_btn, 190, y);
  lv_obj_set_style_border_width(s_reboot_btn, 2, 0);
  lv_obj_set_style_border_color(s_reboot_btn, col(C_LINE), 0);
  lv_obj_align(lv_obj_get_child(s_reboot_btn, 0), LV_ALIGN_LEFT_MID, 24, 0);
}

// ---------- À propos ----------
static void build_about() {
  Page p = make_page("À propos", ev_go_settings, NAV_SET);
  scr_about = p.scr;
  lv_obj_t *c = mk_card(p.body, 760, 300, C_LINE);
  lv_obj_set_pos(c, 260, 40);
  char buf[160];
  lv_obj_set_pos(mk_label(c, FW_NAME, FB24, C_TEXT), 28, 24);
  snprintf(buf, sizeof(buf), "Version %s", FW_VERSION);
  lv_obj_set_pos(mk_label(c, buf, F20, C_ACC_T), 28, 76);
  snprintf(buf, sizeof(buf), "Écran : %d x %d", (int)lv_display_get_horizontal_resolution(nullptr),
           (int)lv_display_get_vertical_resolution(nullptr));
  lv_obj_set_pos(mk_label(c, buf, F18, C_MUTED), 28, 130);
  lv_obj_set_pos(mk_label(c, "M5Stack Tab5 (ESP32-P4)", F18, C_MUTED), 28, 168);
  lv_obj_set_pos(mk_label(c, "Tableau de bord multi-imprimantes Bambu Lab", F18, C_MUTED), 28, 206);
}

// ---------- WiFi ----------
static lv_obj_t *wifi_list, *wifi_status, *wifi_sel_label, *wifi_ta, *wifi_body;
static lv_obj_t *wifi_scan_btn, *wifi_hint, *wifi_show_btn, *wifi_cn_btn, *wifi_other_btn;
static bool g_show_pw = false;
static String g_sel_ssid;
static bool g_scanning = false;

static void ev_show(lv_event_t *e) {
  g_show_pw = !g_show_pw;
  lv_textarea_set_password_mode(wifi_ta, !g_show_pw);
  lv_label_set_text(lv_obj_get_child((lv_obj_t *)lv_event_get_target(e), 0), g_show_pw ? "Masquer" : "Afficher");
}

static void ev_connect(lv_event_t *) {
  if (g_sel_ssid.length() == 0) {
    toast("Choisissez d'abord un réseau");
    return;
  }
  wifi_connect(g_sel_ssid, String(lv_textarea_get_text(wifi_ta)));
}

static void set_wifi_mode(bool pw);

static void ev_pick_net(lv_event_t *e) {
  lv_obj_t *b = (lv_obj_t *)lv_event_get_target(e);
  const char *ssid = (const char *)lv_obj_get_user_data(b);
  g_sel_ssid = ssid;
  char buf[96];
  snprintf(buf, sizeof(buf), "Réseau : %s", ssid);
  lv_label_set_text(wifi_sel_label, buf);
  set_wifi_mode(true);
}

static void fill_net_list(const std::vector<WifiNet> &nets) {
  lv_obj_clean(wifi_list);
  if (nets.empty()) {
    lv_obj_set_pos(mk_label(wifi_list, "Aucun réseau trouvé", F18, C_MUTED), 12, 12);
    return;
  }
  int y = 0;
  for (auto &n : nets) {
    char buf[120];
    snprintf(buf, sizeof(buf), "%s   %d dBm%s", n.ssid.c_str(), n.rssi, n.secured ? "" : "   (ouvert)");
    lv_obj_t *b = mk_button(wifi_list, buf, 420, 54, C_CARD2, C_TEXT, F18, ev_pick_net);
    lv_obj_set_pos(b, 0, y);
    lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 12, 0);
    char *copy = (char *)lv_malloc(n.ssid.length() + 1);
    strcpy(copy, n.ssid.c_str());
    lv_obj_set_user_data(b, copy);
    lv_obj_add_event_cb(b, [](lv_event_t *ee) { lv_free(lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(ee))); },
                        LV_EVENT_DELETE, nullptr);
    y += 62;
  }
}

static void start_scan() {
  if (g_scanning) return;
  g_scanning = true;
  lv_obj_clean(wifi_list);
  lv_obj_set_pos(mk_label(wifi_list, "Recherche en cours...", F18, C_MUTED), 12, 12);
  wifi_scan_start();
}

static void ev_scan(lv_event_t *) { start_scan(); }

static void scan_tick(lv_timer_t *) {
  if (!g_scanning) return;
  std::vector<WifiNet> nets;
  if (wifi_scan_poll(nets)) {
    g_scanning = false;
    fill_net_list(nets);
  }
}

static void ev_other_net(lv_event_t *) { set_wifi_mode(false); }

static void ev_go_wifi(lv_event_t *) {
  set_wifi_mode(false);
  go(scr_wifi);
  start_scan();
}

static void set_wifi_mode(bool pw) {
  auto show = [](lv_obj_t *o, bool on) {
    if (on) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  };
  show(wifi_scan_btn, !pw);
  show(wifi_list, !pw);
  show(wifi_hint, !pw);
  show(wifi_sel_label, pw);
  show(wifi_ta, pw);
  show(wifi_show_btn, pw);
  show(wifi_cn_btn, pw);
  show(wifi_other_btn, pw);
  if (pw) {
    lv_obj_set_pos(wifi_status, 24, 120);
    lv_obj_set_width(wifi_status, 1232);
    kb_attach(wifi_body, 0, KB_Y);
    kb_set_target(wifi_ta);
  } else {
    lv_obj_set_pos(wifi_status, 240, 22);
    lv_obj_set_width(wifi_status, 220);
    kb_hide();
  }
}

static void build_wifi() {
  Page p = make_page("WiFi", ev_go_settings, NAV_SET);
  scr_wifi = p.scr;
  wifi_body = p.body;

  // Mode « liste » : choix du réseau
  wifi_scan_btn = mk_button(p.body, "Rechercher", 200, 48, C_CARD2, C_TEXT, F18, ev_scan);
  lv_obj_set_pos(wifi_scan_btn, 24, 12);
  wifi_status = mk_label(p.body, "", F18, C_MUTED);
  lv_obj_set_pos(wifi_status, 240, 22);
  lv_obj_set_width(wifi_status, 220);
  lv_label_set_long_mode(wifi_status, LV_LABEL_LONG_DOT);

  wifi_list = lv_obj_create(p.body);
  plain(wifi_list, C_CARD);
  lv_obj_set_style_radius(wifi_list, 14, 0);
  lv_obj_set_size(wifi_list, 440, 480);
  lv_obj_set_pos(wifi_list, 24, 72);
  lv_obj_set_style_pad_all(wifi_list, 10, 0);
  lv_obj_add_flag(wifi_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(wifi_list, LV_DIR_VER);

  wifi_hint = mk_label(p.body, "Touchez un réseau pour saisir son mot de passe.", F20, C_MUTED);
  lv_obj_set_pos(wifi_hint, 500, 90);

  // Mode « mot de passe » : champ en haut, clavier pleine largeur en bas
  wifi_sel_label = mk_label(p.body, "Réseau : (aucun)", FB20, C_TEXT);
  lv_obj_set_pos(wifi_sel_label, 24, 10);

  wifi_ta = lv_textarea_create(p.body);
  style_ta(wifi_ta);
  lv_textarea_set_password_mode(wifi_ta, true);
  lv_textarea_set_placeholder_text(wifi_ta, "Mot de passe");
  lv_obj_set_size(wifi_ta, 600, 56);
  lv_obj_set_pos(wifi_ta, 24, 48);
  lv_obj_add_event_cb(wifi_ta, [](lv_event_t *) { kb_show(); }, LV_EVENT_CLICKED, nullptr);

  wifi_show_btn = mk_button(p.body, "Afficher", 170, 56, C_CARD2, C_TEXT, F18, ev_show);
  lv_obj_set_pos(wifi_show_btn, 636, 48);
  wifi_cn_btn = mk_button(p.body, "Se connecter", 270, 56, C_ACC, C_ON_ACC, FB20, ev_connect);
  lv_obj_set_pos(wifi_cn_btn, 818, 48);
  wifi_other_btn = mk_button(p.body, "Autre réseau", 170, 56, C_CARD2, C_TEXT, F18, ev_other_net);
  lv_obj_set_pos(wifi_other_btn, 1100, 48);

  kb_build(p.body, 0, KB_Y);
  set_wifi_mode(false);

  lv_timer_create(scan_tick, 400, nullptr);
}

// ---------- Barre d'état et rafraîchissement ----------
static void status_tick(lv_timer_t *) {
  char buf[160];
  String ssid = wifi_current_ssid();
  int bat = M5.Power.getBatteryLevel();
  if (wifi_state() == WifiState::Connected && ssid.length())
    snprintf(buf, sizeof(buf), "WiFi : %s   |   Batterie %d %%", ssid.c_str(), bat);
  else if (wifi_state() == WifiState::Connecting)
    snprintf(buf, sizeof(buf), "WiFi : connexion...   |   Batterie %d %%", bat);
  else
    snprintf(buf, sizeof(buf), "WiFi : non connecté   |   Batterie %d %%", bat);
  ui_status_apply(buf);

  if (wifi_status) {
    switch (wifi_state()) {
      case WifiState::Connected:
        snprintf(buf, sizeof(buf), "Connecté\n%s", wifi_ip().c_str());
        lv_obj_set_style_text_color(wifi_status, col(C_ACC_T), 0);
        break;
      case WifiState::Connecting:
        snprintf(buf, sizeof(buf), "Connexion...");
        lv_obj_set_style_text_color(wifi_status, col(C_MUTED), 0);
        break;
      case WifiState::Failed:
        snprintf(buf, sizeof(buf), "Échec (mot de passe ?)");
        lv_obj_set_style_text_color(wifi_status, col(C_ERR), 0);
        break;
      default:
        buf[0] = 0;
        break;
    }
    lv_label_set_text(wifi_status, buf);
  }
  home_update();
  alerts_tick();
}

void ui_init() {
  build_home();
  build_settings();
  build_about();
  build_wifi();
  ui_printers_build();
  ui_detail_build();
  ui_tiger_build();
  ui_display_build();
  lv_timer_create(status_tick, 1000, nullptr);
  status_tick(nullptr);

  if (!wifi_has_saved()) {
    ev_go_wifi(nullptr);  // premier démarrage : configuration du WiFi
  } else {
    go(scr_home);
  }
}
