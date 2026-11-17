#include "ui_common.h"
#include <vector>
#include "fonts.h"

lv_obj_t *scr_home, *scr_settings, *scr_wifi, *scr_about, *scr_printers, *scr_wizard, *scr_detail, *scr_alerts, *scr_tiger, *scr_inventory;
static std::vector<lv_obj_t *> status_labels;

lv_color_t col(uint32_t c) { return lv_color_hex(c); }

void plain(lv_obj_t *o, uint32_t bg) {
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_bg_color(o, col(bg), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, const lv_font_t *f, uint32_t c) {
  lv_obj_t *l = lv_label_create(parent);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, col(c), 0);
  return l;
}

lv_obj_t *mk_card(lv_obj_t *parent, int w, int h, uint32_t border) {
  lv_obj_t *c = lv_obj_create(parent);
  plain(c, C_CARD);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_border_width(c, 2, 0);
  lv_obj_set_style_border_color(c, col(border), 0);
  return c;
}

lv_obj_t *mk_button(lv_obj_t *parent, const char *txt, int w, int h, uint32_t bg, uint32_t fg, const lv_font_t *f,
                    lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, col(bg), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = mk_label(b, txt, f, fg);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  return b;
}

void style_ta(lv_obj_t *ta) {
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_style_text_font(ta, F20, 0);
  lv_obj_set_style_bg_color(ta, col(C_CARD), 0);
  lv_obj_set_style_text_color(ta, col(C_TEXT), 0);
  lv_obj_set_style_border_color(ta, col(C_LINE2), 0);
  lv_obj_set_style_border_width(ta, 2, 0);
}

void toast(const char *txt) {
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

void go(lv_obj_t *s) { lv_screen_load(s); }
void ev_go_home(lv_event_t *) { go(scr_home); }
void ev_go_settings(lv_event_t *) { go(scr_settings); }

static void ev_go_inventory(lv_event_t *) { inventory_open(); }

Page make_page(const char *title, lv_event_cb_t back_cb, int nav_active) {
  Page p;
  p.scr = lv_obj_create(nullptr);
  plain(p.scr, C_BG);

  lv_obj_t *top = lv_obj_create(p.scr);
  plain(top, C_NAV);
  lv_obj_set_size(top, SCR_W, TOP_H);
  lv_obj_set_pos(top, 0, 0);
  int x = 24;
  if (back_cb) {
    lv_obj_t *b = mk_button(top, "< Retour", 150, 44, C_CARD2, C_TEXT, F18, back_cb);
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
  struct Tab {
    const char *txt;
    lv_event_cb_t cb;
  };
  Tab tabs[3] = {{"Accueil", ev_go_home}, {"Inventaire", ev_go_inventory}, {"Réglages", ev_go_settings}};
  for (int i = 0; i < 3; i++) {
    bool on = (i == nav_active);
    lv_obj_t *b = mk_button(nav, tabs[i].txt, 260, 56, on ? C_ACC : C_CARD2, on ? C_ON_ACC : C_TEXT, FB20, tabs[i].cb);
    lv_obj_set_pos(b, 220 + i * 290, 12);
  }
  return p;
}

void ui_status_apply(const char *text) {
  for (auto *l : status_labels) lv_label_set_text(l, text);
}

// ---------- Clavier ----------
struct KeyInfo {
  lv_obj_t *btn;
  char base;  // caractère de la couche normale
  char sym;   // caractère de la couche symboles
};
static std::vector<KeyInfo> keys;
static std::vector<lv_obj_t *> lock_btns, sym_btns;
static bool g_shift = false;
static bool g_lock = false;
static bool g_sym = false;
static lv_obj_t *g_target = nullptr;

static bool upper() { return g_shift || g_lock; }

static char key_char(const KeyInfo &k) {
  char c = g_sym ? k.sym : k.base;
  if (upper() && c >= 'a' && c <= 'z') c -= 32;
  return c;
}

static void refresh_keys() {
  for (auto &k : keys) {
    char s[2] = {key_char(k), 0};
    lv_label_set_text(lv_obj_get_child(k.btn, 0), s);
  }
  for (auto *b : lock_btns) lv_obj_set_style_bg_color(b, col(g_lock ? C_ACC : C_LINE2), 0);
  for (auto *b : sym_btns) lv_obj_set_style_bg_color(b, col(g_sym ? C_ACC : C_LINE2), 0);
}

void kb_set_target(lv_obj_t *ta) { g_target = ta; }

static void ev_key(lv_event_t *e) {
  size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
  if (idx >= keys.size()) return;
  if (g_target) lv_textarea_add_char(g_target, key_char(keys[idx]));
  if (g_shift) {
    g_shift = false;
    refresh_keys();
  }
}
static void ev_shift(lv_event_t *) {
  g_shift = !g_shift;
  refresh_keys();
}
static void ev_lock(lv_event_t *) {
  g_lock = !g_lock;
  g_shift = false;
  refresh_keys();
}
static void ev_sym(lv_event_t *) {
  g_sym = !g_sym;
  refresh_keys();
}
static void ev_space(lv_event_t *) {
  if (g_target) lv_textarea_add_char(g_target, ' ');
}
static void ev_del(lv_event_t *) {
  if (g_target) lv_textarea_delete_char(g_target);
}
static void ev_clear(lv_event_t *) {
  if (g_target) lv_textarea_set_text(g_target, "");
}

static void add_key_row(lv_obj_t *parent, const char *base, const char *sym, int x0, int y) {
  int x = x0;
  for (int i = 0; base[i]; i++) {
    char s[2] = {base[i], 0};
    size_t idx = keys.size();
    lv_obj_t *b = mk_button(parent, s, 66, 60, C_CARD2, C_TEXT, FB20, ev_key, (void *)(uintptr_t)idx);
    lv_obj_set_pos(b, x, y);
    keys.push_back({b, base[i], sym[i]});
    x += 72;
  }
}

void kb_build(lv_obj_t *parent, int x, int y) {
  add_key_row(parent, "1234567890", "!@#$%^&*()", x, y);
  add_key_row(parent, "azertyuiop", "-_=+[]{}\\|", x, y + 68);
  add_key_row(parent, "qsdfghjklm", ";:'\",<>/?`", x, y + 136);
  add_key_row(parent, "wxcvbn.-_@", "wxcvbn.-_@", x, y + 204);
  int r = y + 276;
  lv_obj_t *sh = mk_button(parent, "Maj", 90, 60, C_LINE2, C_TEXT, F18, ev_shift);
  lv_obj_set_pos(sh, x, r);
  lv_obj_t *lk = mk_button(parent, "Verr. Maj", 120, 60, C_LINE2, C_TEXT, F16, ev_lock);
  lv_obj_set_pos(lk, x + 96, r);
  lock_btns.push_back(lk);
  lv_obj_t *sy = mk_button(parent, "Symb.", 100, 60, C_LINE2, C_TEXT, F18, ev_sym);
  lv_obj_set_pos(sy, x + 222, r);
  sym_btns.push_back(sy);
  lv_obj_t *sp = mk_button(parent, "Espace", 150, 60, C_CARD2, C_TEXT, F18, ev_space);
  lv_obj_set_pos(sp, x + 328, r);
  lv_obj_t *dl = mk_button(parent, "Effacer", 120, 60, C_LINE2, C_TEXT, F18, ev_del);
  lv_obj_set_pos(dl, x + 484, r);
  lv_obj_t *cl = mk_button(parent, "Vider", 100, 60, C_LINE2, C_TEXT, F18, ev_clear);
  lv_obj_set_pos(cl, x + 610, r);
}

// ---------- Libellés ----------
const char *link_text(int link, const char *gcode, bool got_report, int error) {
  // link : 0 Off, 1 Connecting, 2 Connected, 3 AuthFailed, 4 Unreachable (voir enum Link)
  if (link == 3) return "Code d'accès refusé";
  if (link == 4) return "Injoignable";
  if (link == 0) return "WiFi non connecté";
  if (link == 1 || !got_report) return "Connexion...";
  if (error != 0) return "Erreur imprimante";
  String g = gcode;
  if (g == "RUNNING") return "Impression en cours";
  if (g == "PAUSE") return "En pause";
  if (g == "FINISH") return "Terminée";
  if (g == "FAILED") return "Échec de l'impression";
  if (g == "PREPARE") return "Préparation";
  return "Au repos";
}

String fmt_remaining(int minutes) {
  if (minutes < 0) return "-";
  if (minutes < 60) return String(minutes) + " min";
  return String(minutes / 60) + " h " + (minutes % 60 < 10 ? "0" : "") + String(minutes % 60);
}
