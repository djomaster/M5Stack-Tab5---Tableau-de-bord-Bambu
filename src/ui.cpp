#include "ui.h"
#include <M5Unified.h>
#include <lvgl.h>
#include <vector>
#include "fonts.h"
#include "version.h"
#include "wifi_manager.h"

// ---------- Couleurs (maquette approuvée) ----------
#define C_BG 0x0F1115
#define C_NAV 0x12151B
#define C_CARD 0x171A21
#define C_CARD2 0x1D222B
#define C_LINE 0x262B35
#define C_LINE2 0x343B48
#define C_TEXT 0xE8EAEE
#define C_MUTED 0xA7AFBD
#define C_ACC 0x2DD4A7
#define C_ACC_T 0x5EEAC2
#define C_WARN 0xFB923C
#define C_WARN_T 0xFDBA74
#define C_ERR 0xF87171

static const int SCR_W = 1280;
static const int TOP_H = 64;
static const int NAV_H = 80;

static lv_color_t col(uint32_t c) { return lv_color_hex(c); }

// ---------- Écrans ----------
static lv_obj_t *scr_home, *scr_settings, *scr_wifi, *scr_about;
static std::vector<lv_obj_t *> status_labels;  // barre du haut de chaque écran

struct Page {
  lv_obj_t *scr;
  lv_obj_t *body;
};

// ---------- Utilitaires ----------
static void plain(lv_obj_t *o, uint32_t bg) {
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_bg_color(o, col(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, const lv_font_t *f, uint32_t c) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, col(c), 0);
  return l;
}

static lv_obj_t *mk_card(lv_obj_t *parent, int w, int h, uint32_t border) {
  lv_obj_t *c = lv_obj_create(parent);
  plain(c, C_CARD);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_border_color(c, col(border), 0);
  return c;
}

static lv_obj_t *mk_button(lv_obj_t *parent, const char *txt, int w, int h, uint32_t bg, uint32_t fg,
                           const lv_font_t *f, lv_event_cb_t cb, void *ud = nullptr) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, col(bg), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = mk_label(b, txt, f, fg);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

static void toast(const char *txt) {
  lv_obj_t *t = lv_obj_create(lv_layer_top());
  plain(t, C_CARD2);
  lv_obj_set_style_radius(t, 14, 0);
  lv_obj_set_style_border_width(t, 2, 0);
  lv_obj_set_style_border_color(t, col(C_LINE2), 0);
  lv_obj_set_size(t, 640, 64);
  lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -110);
  lv_obj_t *l = mk_label(t, txt, F20, C_TEXT);
  lv_obj_center(l);
  lv_timer_t *tm = lv_timer_create([](lv_timer_t *tt) { lv_obj_delete((lv_obj_t *)lv_timer_get_user_data(tt)); }, 2200, t);
  lv_timer_set_repeat_count(tm, 1);
}

static void go(lv_obj_t *s) { lv_screen_load(s); }

static void ev_go_home(lv_event_t *) { go(scr_home); }
static void ev_go_settings(lv_event_t *) { go(scr_settings); }
static void ev_go_wifi(lv_event_t *);
static void ev_go_about(lv_event_t *) { go(scr_about); }
static void ev_soon(lv_event_t *) { toast("Disponible dans une prochaine version"); }

// Écran type : barre du haut (titre, état), zone centrale, barre de navigation.
static Page make_page(const char *title, bool back, bool nav_home_active) {
  Page p;
  p.scr = lv_obj_create(nullptr);
  plain(p.scr, C_BG);

  lv_obj_t *top = lv_obj_create(p.scr);
  plain(top, C_NAV);
  lv_obj_set_size(top, SCR_W, TOP_H);
  lv_obj_set_pos(top, 0, 0);
  int x = 24;
  if (back) {
    lv_obj_t *b = mk_button(top, "< Retour", 150, 44, C_CARD2, C_TEXT, F18, ev_go_settings);
    lv_obj_set_pos(b, 16, 10);
    x = 190;
  }
  lv_obj_t *t = mk_label(top, title, FB24, C_TEXT);
  lv_obj_set_pos(t, x, 16);
  lv_obj_t *st = mk_label(top, "", F18, C_MUTED);
  lv_obj_set_width(st, 600);
  lv_obj_set_style_text_align(st, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(st, SCR_W - 624, 20);
  status_labels.push_back(st);

  p.body = lv_obj_create(p.scr);
  plain(p.body, C_BG);
  lv_obj_set_size(p.body, SCR_W, 720 - TOP_H - NAV_H);
  lv_obj_set_pos(p.body, 0, TOP_H);

  lv_obj_t *nav = lv_obj_create(p.scr);
  plain(nav, C_NAV);
  lv_obj_set_size(nav, SCR_W, NAV_H);
  lv_obj_set_pos(nav, 0, 720 - NAV_H);
  lv_obj_t *b1 = mk_button(nav, "Accueil", 300, 56, nav_home_active ? C_ACC : C_CARD2,
                           nav_home_active ? 0x06281F : C_TEXT, FB20, ev_go_home);
  lv_obj_set_pos(b1, 330, 12);
  lv_obj_t *b2 = mk_button(nav, "Réglages", 300, 56, nav_home_active ? C_CARD2 : C_ACC,
                           nav_home_active ? C_TEXT : 0x06281F, FB20, ev_go_settings);
  lv_obj_set_pos(b2, 650, 12);
  return p;
}

// ---------- Accueil (données d'exemple) ----------
static void build_home() {
  Page p = make_page("Tableau de bord", false, true);
  scr_home = p.scr;

  lv_obj_t *note = mk_label(p.body, "Données d'exemple : aucune imprimante n'est encore connectée", F16, C_MUTED);
  lv_obj_set_pos(note, 24, 14);

  // Carte 1 : impression en cours
  lv_obj_t *c1 = mk_card(p.body, 400, 440, C_ACC);
  lv_obj_set_pos(c1, 24, 56);
  mk_label(c1, "Bambu X1C", FB24, C_TEXT);
  lv_obj_set_pos(lv_obj_get_child(c1, 0), 24, 20);
  lv_obj_t *s1 = mk_label(c1, "Impression en cours", F18, C_ACC_T);
  lv_obj_set_pos(s1, 24, 60);
  lv_obj_t *pct = mk_label(c1, "64 %", FB28, C_TEXT);
  lv_obj_set_pos(pct, 24, 120);
  lv_obj_t *bar = lv_bar_create(c1);
  lv_obj_set_size(bar, 352, 18);
  lv_obj_set_pos(bar, 24, 180);
  lv_obj_set_style_bg_color(bar, col(C_LINE), 0);
  lv_obj_set_style_bg_color(bar, col(C_ACC), LV_PART_INDICATOR);
  lv_bar_set_value(bar, 64, LV_ANIM_OFF);
  mk_label(c1, "Support_caméra.3mf", F18, C_TEXT);
  lv_obj_set_pos(lv_obj_get_child(c1, 4), 24, 224);
  mk_label(c1, "Couche 142 / 220", F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c1, 5), 24, 262);
  mk_label(c1, "Temps restant : 1 h 12", F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c1, 6), 24, 298);
  mk_label(c1, "Buse 220 °C   Plateau 60 °C", F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c1, 7), 24, 334);

  // Carte 2 : erreur
  lv_obj_t *c2 = mk_card(p.body, 400, 440, C_WARN);
  lv_obj_set_pos(c2, 440, 56);
  mk_label(c2, "Bambu H2C", FB24, C_TEXT);
  lv_obj_set_pos(lv_obj_get_child(c2, 0), 24, 20);
  mk_label(c2, "Attention", FB20, C_WARN_T);
  lv_obj_set_pos(lv_obj_get_child(c2, 1), 24, 60);
  mk_label(c2, "Filament épuisé", F24, C_TEXT);
  lv_obj_set_pos(lv_obj_get_child(c2, 2), 24, 120);
  mk_label(c2, "Impression en pause à 37 %", F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c2, 3), 24, 170);
  mk_label(c2, "Remplacez la bobine puis reprenez.", F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c2, 4), 24, 208);

  // Carte 3 : ajouter une imprimante
  lv_obj_t *c3 = mk_card(p.body, 400, 440, C_LINE2);
  lv_obj_set_pos(c3, 856, 56);
  lv_obj_set_style_bg_opa(c3, LV_OPA_50, 0);
  mk_label(c3, "+", FB28, C_MUTED);
  lv_obj_align(lv_obj_get_child(c3, 0), LV_ALIGN_CENTER, 0, -30);
  mk_label(c3, "Ajouter une imprimante", F20, C_MUTED);
  lv_obj_align(lv_obj_get_child(c3, 1), LV_ALIGN_CENTER, 0, 20);
  lv_obj_add_flag(c3, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c3, ev_soon, LV_EVENT_CLICKED, nullptr);
}

// ---------- Réglages ----------
static void build_settings() {
  Page p = make_page("Réglages", false, false);
  scr_settings = p.scr;
  struct Item { const char *txt; lv_event_cb_t cb; };
  Item items[] = {
      {"WiFi", ev_go_wifi},
      {"Imprimantes", ev_soon},
      {"Alertes", ev_soon},
      {"Affichage", ev_soon},
      {"À propos", ev_go_about},
  };
  int y = 24;
  for (auto &it : items) {
    lv_obj_t *b = mk_button(p.body, it.txt, 900, 80, C_CARD, C_TEXT, FB20, it.cb);
    lv_obj_set_pos(b, 190, y);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, col(C_LINE), 0);
    lv_obj_t *l = lv_obj_get_child(b, 0);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 24, 0);
    y += 96;
  }
}

// ---------- À propos ----------
static void build_about() {
  Page p = make_page("À propos", true, false);
  scr_about = p.scr;
  lv_obj_t *c = mk_card(p.body, 760, 300, C_LINE);
  lv_obj_set_pos(c, 260, 40);
  char buf[160];
  mk_label(c, FW_NAME, FB24, C_TEXT);
  lv_obj_set_pos(lv_obj_get_child(c, 0), 28, 24);
  snprintf(buf, sizeof(buf), "Version %s", FW_VERSION);
  mk_label(c, buf, F20, C_ACC_T);
  lv_obj_set_pos(lv_obj_get_child(c, 1), 28, 76);
  snprintf(buf, sizeof(buf), "Écran : %d x %d", (int)lv_display_get_horizontal_resolution(nullptr),
           (int)lv_display_get_vertical_resolution(nullptr));
  mk_label(c, buf, F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c, 2), 28, 130);
  mk_label(c, "M5Stack Tab5 (ESP32-P4)", F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c, 3), 28, 168);
  mk_label(c, "Tableau de bord multi-imprimantes Bambu Lab", F18, C_MUTED);
  lv_obj_set_pos(lv_obj_get_child(c, 4), 28, 206);
}

// ---------- WiFi ----------
static lv_obj_t *wifi_list, *wifi_status, *wifi_sel_label, *wifi_ta;
static lv_obj_t *wifi_scan_btn_label;
static std::vector<lv_obj_t *> key_btns;
static bool g_shift = false;
static bool g_show_pw = false;
static String g_sel_ssid;
static bool g_scanning = false;
static lv_timer_t *scan_timer = nullptr;

static void refresh_keys() {
  for (auto *b : key_btns) {
    char base = (char)(uintptr_t)lv_obj_get_user_data(b);
    char c = base;
    if (g_shift && base >= 'a' && base <= 'z') c = base - 32;
    char s[2] = {c, 0};
    lv_label_set_text(lv_obj_get_child(b, 0), s);
  }
}

static void ev_key(lv_event_t *e) {
  lv_obj_t *b = (lv_obj_t *)lv_event_get_target(e);
  char c = (char)(uintptr_t)lv_obj_get_user_data(b);
  if (g_shift && c >= 'a' && c <= 'z') c -= 32;
  lv_textarea_add_char(wifi_ta, c);
  if (g_shift) {
    g_shift = false;
    refresh_keys();
  }
}

static void ev_shift(lv_event_t *) {
  g_shift = !g_shift;
  refresh_keys();
}
static void ev_space(lv_event_t *) { lv_textarea_add_char(wifi_ta, ' '); }
static void ev_del(lv_event_t *) { lv_textarea_delete_char(wifi_ta); }
static void ev_clear(lv_event_t *) { lv_textarea_set_text(wifi_ta, ""); }
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

static void ev_pick_net(lv_event_t *e) {
  lv_obj_t *b = (lv_obj_t *)lv_event_get_target(e);
  const char *ssid = (const char *)lv_obj_get_user_data(b);
  g_sel_ssid = ssid;
  char buf[96];
  snprintf(buf, sizeof(buf), "Réseau : %s", ssid);
  lv_label_set_text(wifi_sel_label, buf);
}

static void fill_net_list(const std::vector<WifiNet> &nets) {
  lv_obj_clean(wifi_list);
  if (nets.empty()) {
    lv_obj_t *l = mk_label(wifi_list, "Aucun réseau trouvé", F18, C_MUTED);
    lv_obj_set_pos(l, 12, 12);
    return;
  }
  int y = 0;
  for (auto &n : nets) {
    char buf[120];
    snprintf(buf, sizeof(buf), "%s   %d dBm%s", n.ssid.c_str(), n.rssi, n.secured ? "" : "   (ouvert)");
    lv_obj_t *b = mk_button(wifi_list, buf, 420, 54, C_CARD2, C_TEXT, F18, ev_pick_net);
    lv_obj_set_pos(b, 0, y);
    lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 12, 0);
    // Copie du SSID conservée pour la durée de vie du bouton (libérée à la suppression).
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
  lv_obj_t *l = mk_label(wifi_list, "Recherche en cours...", F18, C_MUTED);
  lv_obj_set_pos(l, 12, 12);
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

static void ev_go_wifi(lv_event_t *) {
  go(scr_wifi);
  start_scan();
}

static void add_key_row(lv_obj_t *parent, const char *keys, int y, int x0) {
  int x = x0;
  for (const char *k = keys; *k; k++) {
    char s[2] = {*k, 0};
    lv_obj_t *b = mk_button(parent, s, 66, 60, C_CARD2, C_TEXT, FB20, ev_key);
    lv_obj_set_user_data(b, (void *)(uintptr_t)*k);
    lv_obj_set_pos(b, x, y);
    key_btns.push_back(b);
    x += 72;
  }
}

static void build_wifi() {
  Page p = make_page("WiFi", true, false);
  scr_wifi = p.scr;

  // Gauche : réseaux
  lv_obj_t *scan = mk_button(p.body, "Rechercher", 200, 48, C_CARD2, C_TEXT, F18, ev_scan);
  lv_obj_set_pos(scan, 24, 12);
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

  // Droite : mot de passe + clavier
  wifi_sel_label = mk_label(p.body, "Réseau : (aucun)", FB20, C_TEXT);
  lv_obj_set_pos(wifi_sel_label, 500, 14);

  wifi_ta = lv_textarea_create(p.body);
  lv_textarea_set_one_line(wifi_ta, true);
  lv_textarea_set_password_mode(wifi_ta, true);
  lv_textarea_set_placeholder_text(wifi_ta, "Mot de passe");
  lv_obj_set_size(wifi_ta, 560, 56);
  lv_obj_set_pos(wifi_ta, 500, 52);
  lv_obj_set_style_text_font(wifi_ta, F20, 0);
  lv_obj_set_style_bg_color(wifi_ta, col(C_CARD), 0);
  lv_obj_set_style_text_color(wifi_ta, col(C_TEXT), 0);
  lv_obj_set_style_border_color(wifi_ta, col(C_LINE2), 0);
  lv_obj_set_style_border_width(wifi_ta, 2, 0);

  lv_obj_t *show = mk_button(p.body, "Afficher", 190, 56, C_CARD2, C_TEXT, F18, ev_show);
  lv_obj_set_pos(show, 1076, 52);

  int x0 = 500;
  add_key_row(p.body, "1234567890", 124, x0);
  add_key_row(p.body, "azertyuiop", 192, x0);
  add_key_row(p.body, "qsdfghjklm", 260, x0);
  add_key_row(p.body, "wxcvbn.-_@", 328, x0);

  lv_obj_t *sh = mk_button(p.body, "Maj", 110, 60, C_LINE2, C_TEXT, F18, ev_shift);
  lv_obj_set_pos(sh, 500, 400);
  lv_obj_t *sp = mk_button(p.body, "Espace", 280, 60, C_CARD2, C_TEXT, F18, ev_space);
  lv_obj_set_pos(sp, 618, 400);
  lv_obj_t *dl = mk_button(p.body, "Effacer", 130, 60, C_LINE2, C_TEXT, F18, ev_del);
  lv_obj_set_pos(dl, 906, 400);
  lv_obj_t *cl = mk_button(p.body, "Tout", 130, 60, C_LINE2, C_TEXT, F18, ev_clear);
  lv_obj_set_pos(cl, 1044, 400);

  lv_obj_t *cn = mk_button(p.body, "Se connecter", 400, 64, C_ACC, 0x06281F, FB20, ev_connect);
  lv_obj_set_pos(cn, 500, 480);

  scan_timer = lv_timer_create(scan_tick, 400, nullptr);
}

// ---------- Barre d'état (WiFi + batterie), rafraîchie chaque seconde ----------
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
  for (auto *l : status_labels) lv_label_set_text(l, buf);

  if (wifi_status) {
    switch (wifi_state()) {
      case WifiState::Connected: {
        String ip = wifi_ip();
        snprintf(buf, sizeof(buf), "Connecté\n%s", ip.c_str());
        lv_obj_set_style_text_color(wifi_status, col(C_ACC_T), 0);
        break;
      }
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
}

// ---------- Point d'entrée ----------
void ui_init() {
  build_home();
  build_settings();
  build_about();
  build_wifi();
  lv_timer_create(status_tick, 1000, nullptr);
  status_tick(nullptr);

  if (!wifi_has_saved()) {
    // Premier démarrage : on guide directement vers la configuration WiFi.
    ev_go_wifi(nullptr);
  } else {
    go(scr_home);
  }
}
