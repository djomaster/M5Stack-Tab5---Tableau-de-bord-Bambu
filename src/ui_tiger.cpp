// Écrans Tiger Studio : compte (Réglages > Tiger Studio) et inventaire des bobines (onglet Inventaire).
#include <lvgl.h>
#include <algorithm>
#include <vector>
#include "fonts.h"
#include "tiger.h"
#include "ui_common.h"

// =====================================================================
//  Compte
// =====================================================================
static lv_obj_t *t_email, *t_pass, *t_status, *t_login_btn_label;
static std::vector<lv_obj_t *> t_fields;
static bool t_show_pw = false;

static void focus_field(lv_obj_t *ta) {
  kb_set_target(ta);
  kb_show();
  for (auto *f : t_fields) lv_obj_set_style_border_color(f, col(f == ta ? C_ACC : C_LINE2), 0);
}
static void ev_field(lv_event_t *e) { focus_field((lv_obj_t *)lv_event_get_target(e)); }

static void ev_show_pw(lv_event_t *e) {
  t_show_pw = !t_show_pw;
  lv_textarea_set_password_mode(t_pass, !t_show_pw);
  lv_label_set_text(lv_obj_get_child((lv_obj_t *)lv_event_get_target(e), 0), t_show_pw ? "Masquer" : "Afficher");
}

static void ev_login(lv_event_t *) {
  String email = lv_textarea_get_text(t_email);
  String pass = lv_textarea_get_text(t_pass);
  email.trim();
  if (email.isEmpty() || pass.isEmpty()) {
    toast("Saisissez l'e-mail et le mot de passe");
    return;
  }
  tiger_login(email, pass);
  lv_textarea_set_text(t_pass, "");  // le mot de passe ne reste pas à l'écran
}

static void ev_logout(lv_event_t *) {
  tiger_logout();
  toast("Compte déconnecté");
}
static void ev_sync(lv_event_t *) {
  if (!tiger_signed_in()) {
    toast("Connectez d'abord votre compte");
    return;
  }
  tiger_sync_now();
}

static lv_obj_t *t_body;

void tiger_screen_open() {
  kb_attach(t_body, 0, KB_Y);
  focus_field(t_email);
  go(scr_tiger);
}

static void build_tiger() {
  Page p = make_page("Tiger Studio", ev_go_settings, NAV_SET);
  scr_tiger = p.scr;
  t_body = p.body;

  lv_obj_set_pos(mk_label(p.body, "Adresse e-mail TigerTag", F18, C_MUTED), 24, 6);
  t_email = lv_textarea_create(p.body);
  style_ta(t_email);
  lv_textarea_set_placeholder_text(t_email, "vous@exemple.fr");
  lv_obj_set_size(t_email, 540, 52);
  lv_obj_set_pos(t_email, 24, 34);
  lv_obj_add_event_cb(t_email, ev_field, LV_EVENT_CLICKED, nullptr);
  t_fields.push_back(t_email);

  lv_obj_set_pos(mk_label(p.body, "Mot de passe", F18, C_MUTED), 580, 6);
  t_pass = lv_textarea_create(p.body);
  style_ta(t_pass);
  lv_textarea_set_password_mode(t_pass, true);
  lv_textarea_set_placeholder_text(t_pass, "Mot de passe");
  lv_obj_set_size(t_pass, 440, 52);
  lv_obj_set_pos(t_pass, 580, 34);
  lv_obj_add_event_cb(t_pass, ev_field, LV_EVENT_CLICKED, nullptr);
  t_fields.push_back(t_pass);

  lv_obj_set_pos(mk_button(p.body, "Afficher", 150, 52, C_CARD2, C_TEXT, F18, ev_show_pw), 1032, 34);
  lv_obj_set_pos(mk_button(p.body, "Se connecter", 300, 54, C_ACC, C_ON_ACC, FB20, ev_login), 24, 104);
  lv_obj_set_pos(mk_button(p.body, "Synchroniser", 250, 54, C_CARD2, C_TEXT, F18, ev_sync), 336, 104);
  lv_obj_set_pos(mk_button(p.body, "Se déconnecter", 250, 54, C_CARD2, C_TEXT, F18, ev_logout), 598, 104);

  t_status = mk_label(p.body, "", F16, C_MUTED);
  lv_obj_set_width(t_status, 380);
  lv_label_set_long_mode(t_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(t_status, 872, 98);

  kb_build(p.body, 0, KB_Y);
  focus_field(t_email);
}

static void tiger_tick(lv_timer_t *) {
  if (lv_screen_active() != scr_tiger) return;
  char buf[200];
  uint32_t c = C_MUTED;
  switch (tiger_state()) {
    case TigerState::SignedOut: {
      String m = tiger_message();
      snprintf(buf, sizeof(buf), "%s", m.length() ? m.c_str() : "Non connecté");
      if (m.length()) c = C_ERR;
      break;
    }
    case TigerState::SigningIn: snprintf(buf, sizeof(buf), "Connexion en cours..."); break;
    case TigerState::Syncing: snprintf(buf, sizeof(buf), "Synchronisation de l'inventaire..."); break;
    case TigerState::Ready:
      snprintf(buf, sizeof(buf), "Connecté : %s\n%d bobines dans l'inventaire", tiger_email().c_str(), tiger_count());
      c = C_ACC_T;
      break;
    case TigerState::Error: {
      String m = tiger_message();
      snprintf(buf, sizeof(buf), "Erreur : %s\n(nouvel essai automatique dans une minute)", m.c_str());
      c = C_WARN_T;
      break;
    }
  }
  lv_label_set_text(t_status, buf);
  lv_obj_set_style_text_color(t_status, col(c), 0);
}

// =====================================================================
//  Inventaire
// =====================================================================
struct Row {
  Spool s;
  String brand, material;
};
static lv_obj_t *i_list, *i_summary, *i_filter_label, *i_empty;
static std::vector<String> i_materials;  // matières présentes, pour le filtre
static int i_filter = -1;                // -1 = toutes
static int i_shown_rev = -1;
static int i_shown_filter = -2;
static const int LOW_G = 100;  // seuil de stock bas, en grammes

static void ev_open_tiger(lv_event_t *) { tiger_screen_open(); }

static void ev_filter(lv_event_t *) {
  i_filter++;
  if (i_filter >= (int)i_materials.size()) i_filter = -1;
  i_shown_filter = -2;  // force le redessin au prochain passage
}
static void ev_inv_sync(lv_event_t *) {
  if (!tiger_signed_in()) {
    toast("Connectez d'abord votre compte (Réglages > Tiger Studio)");
    return;
  }
  tiger_sync_now();
}

static void rebuild_inventory() {
  lv_obj_clean(i_list);
  std::vector<Row> rows;
  int n = tiger_count();
  for (int i = 0; i < n; i++) {
    Row r;
    if (!tiger_get(i, r.s)) continue;
    r.brand = tiger_brand(r.s.brand);
    r.material = tiger_material(r.s.material);
    rows.push_back(r);
  }
  // matières présentes
  i_materials.clear();
  for (auto &r : rows)
    if (std::find(i_materials.begin(), i_materials.end(), r.material) == i_materials.end()) i_materials.push_back(r.material);
  std::sort(i_materials.begin(), i_materials.end());
  if (i_filter >= (int)i_materials.size()) i_filter = -1;
  String flt = i_filter >= 0 ? i_materials[i_filter] : String("");

  std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
    if (a.material != b.material) return a.material < b.material;
    if (a.brand != b.brand) return a.brand < b.brand;
    return a.s.color_name < b.s.color_name;
  });

  int shown = 0;
  long total_g = 0;
  int y = 0;
  for (auto &r : rows) {
    if (flt.length() && r.material != flt) continue;
    lv_obj_t *row = lv_obj_create(i_list);
    plain(row, C_CARD);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_set_size(row, 1200, 62);
    lv_obj_set_pos(row, 0, y);
    lv_obj_t *sw = lv_obj_create(row);
    plain(sw, r.s.color);
    lv_obj_set_size(sw, 34, 34);
    lv_obj_set_style_radius(sw, 17, 0);
    lv_obj_set_style_border_width(sw, 2, 0);
    lv_obj_set_style_border_color(sw, col(C_LINE2), 0);
    lv_obj_set_pos(sw, 16, 14);
    String title = r.brand + "   " + r.material;
    lv_obj_t *t = mk_label(row, title.c_str(), FB18, C_TEXT);
    lv_obj_set_width(t, 520);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(t, 70, 8);
    lv_obj_t *cn = mk_label(row, r.s.color_name.length() ? r.s.color_name.c_str() : "-", F16, C_MUTED);
    lv_obj_set_width(cn, 520);
    lv_label_set_long_mode(cn, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(cn, 70, 34);
    char w[40];
    bool low = r.s.weight >= 0 && r.s.weight < LOW_G;
    if (r.s.weight >= 0) snprintf(w, sizeof(w), "%d g", r.s.weight);
    else snprintf(w, sizeof(w), "-");
    lv_obj_t *wl = mk_label(row, w, FB20, low ? C_WARN_T : C_TEXT);
    lv_obj_set_width(wl, 200);
    lv_obj_set_style_text_align(wl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(wl, 970, 18);
    if (r.s.weight > 0) total_g += r.s.weight;
    y += 70;
    shown++;
  }
  char buf[120];
  snprintf(buf, sizeof(buf), "%d bobines   |   %.1f kg de filament", shown, total_g / 1000.0);
  lv_label_set_text(i_summary, buf);
  lv_label_set_text(i_filter_label, flt.length() ? ("Matière : " + flt).c_str() : "Matière : toutes");
  if (rows.empty()) {
    lv_label_set_text(i_empty, tiger_signed_in() ? "Aucune bobine dans l'inventaire." : "Compte Tiger Studio non connecté.");
    lv_obj_remove_flag(i_empty, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(i_empty, LV_OBJ_FLAG_HIDDEN);
  }
}

static void inv_tick(lv_timer_t *) {
  if (lv_screen_active() != scr_inventory) return;
  if (tiger_revision() != i_shown_rev || i_filter != i_shown_filter) {
    i_shown_rev = tiger_revision();
    i_shown_filter = i_filter;
    rebuild_inventory();
  }
}

void inventory_open() {
  i_shown_rev = -1;
  go(scr_inventory);
  inv_tick(nullptr);
}

static void build_inventory() {
  Page p = make_page("Inventaire", nullptr, NAV_INV);
  scr_inventory = p.scr;
  i_summary = mk_label(p.body, "", F18, C_MUTED);
  lv_obj_set_pos(i_summary, 24, 18);
  lv_obj_t *fb = mk_button(p.body, "Matière : toutes", 300, 44, C_CARD2, C_TEXT, F18, ev_filter);
  lv_obj_set_pos(fb, 620, 8);
  i_filter_label = lv_obj_get_child(fb, 0);
  lv_obj_set_pos(mk_button(p.body, "Synchroniser", 170, 44, C_CARD2, C_TEXT, F18, ev_inv_sync), 934, 8);
  lv_obj_set_pos(mk_button(p.body, "Compte", 130, 44, C_CARD2, C_TEXT, F18, ev_open_tiger), 1114, 8);

  i_list = lv_obj_create(p.body);
  plain(i_list, C_BG);
  lv_obj_set_size(i_list, 1240, 500);
  lv_obj_set_pos(i_list, 20, 62);
  lv_obj_add_flag(i_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(i_list, LV_DIR_VER);

  i_empty = mk_label(p.body, "", F20, C_MUTED);
  lv_obj_set_pos(i_empty, 40, 100);
  lv_obj_add_flag(i_empty, LV_OBJ_FLAG_HIDDEN);
}

void ui_tiger_build() {
  build_tiger();
  build_inventory();
  lv_timer_create(tiger_tick, 1000, nullptr);
  lv_timer_create(inv_tick, 1000, nullptr);
}
