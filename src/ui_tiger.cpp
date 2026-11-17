// Écrans Tiger Studio : compte (Réglages > Tiger Studio) et inventaire des bobines (onglet Inventaire).
#include <lvgl.h>
#include <algorithm>
#include <vector>
#include "fonts.h"
#include "diag.h"
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
  String brand, material, family;
};
static lv_obj_t *i_list, *i_summary, *i_empty, *i_fb_brand, *i_fb_mat, *i_fb_col, *i_picker = nullptr;
static String f_brand, f_mat, f_col;  // filtres actifs ("" = tous)
static int i_shown_rev = -1;
static bool i_dirty = true;
static const int LOW_G = 100;  // seuil de stock bas, en grammes
enum { F_BRAND = 0, F_MAT = 1, F_COL = 2 };

static void ev_open_tiger(lv_event_t *) { tiger_screen_open(); }

// Famille de couleur d'une bobine (pour le filtre), déduite de son code RVB.
static const char *COLOR_FAMILIES[] = {"Noir", "Blanc", "Gris", "Rouge", "Orange", "Marron", "Jaune", "Vert", "Cyan", "Bleu", "Violet", "Rose"};
static String color_family(uint32_t rgb) {
  float r = ((rgb >> 16) & 255) / 255.0f, g = ((rgb >> 8) & 255) / 255.0f, b = (rgb & 255) / 255.0f;
  float mx = fmaxf(r, fmaxf(g, b)), mn = fminf(r, fminf(g, b)), d = mx - mn;
  float v = mx, s = mx <= 0 ? 0 : d / mx, h = 0;
  if (d > 0) {
    if (mx == r) h = 60 * fmodf((g - b) / d, 6);
    else if (mx == g) h = 60 * ((b - r) / d + 2);
    else h = 60 * ((r - g) / d + 4);
    if (h < 0) h += 360;
  }
  if (v < 0.18f) return "Noir";
  if (s < 0.14f) return v > 0.85f ? "Blanc" : "Gris";
  if (h < 15 || h >= 345) return s < 0.5f && v > 0.7f ? "Rose" : "Rouge";
  if (h < 45) return v < 0.65f ? "Marron" : "Orange";
  if (h < 70) return "Jaune";
  if (h < 165) return "Vert";
  if (h < 200) return "Cyan";
  if (h < 255) return "Bleu";
  if (h < 295) return "Violet";
  return "Rose";
}

static void ev_inv_sync(lv_event_t *) {
  if (!tiger_signed_in()) {
    toast("Connectez d'abord votre compte (Réglages > Tiger Studio)");
    return;
  }
  tiger_sync_now();
}

static void load_rows(std::vector<Row> &rows) {
  int n = tiger_count();
  for (int i = 0; i < n; i++) {
    Row r;
    if (!tiger_get(i, r.s)) continue;
    r.brand = tiger_brand(r.s.brand);
    r.material = tiger_material(r.s.material);
    r.family = color_family(r.s.color);
    rows.push_back(r);
  }
}

static String &filter_ref(int kind) { return kind == F_BRAND ? f_brand : (kind == F_MAT ? f_mat : f_col); }

// ---- Sélecteur : fenêtre avec la liste des valeurs possibles ----
static void picker_close() {
  if (i_picker) {
    lv_obj_delete_async(i_picker);
    i_picker = nullptr;
  }
}
static void ev_picker_close(lv_event_t *) { picker_close(); }
struct PickOpt {
  int kind;
  String value;
};
static std::vector<PickOpt> g_opts;  // une entrée par bouton (durée de vie : celle du sélecteur)
static void ev_pick(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx >= 0 && idx < (int)g_opts.size()) {
    filter_ref(g_opts[idx].kind) = g_opts[idx].value;
    i_dirty = true;
  }
  picker_close();
}
static void ev_reset_filters(lv_event_t *) {
  f_brand = f_mat = f_col = "";
  i_dirty = true;
}

static void picker_open(int kind) {
  picker_close();
  std::vector<Row> rows;
  load_rows(rows);
  std::vector<std::pair<String, int>> vals;  // valeur, nombre de bobines
  auto add = [&](const String &v) {
    for (auto &p : vals)
      if (p.first == v) {
        p.second++;
        return;
      }
    vals.push_back({v, 1});
  };
  for (auto &r : rows) add(kind == F_BRAND ? r.brand : (kind == F_MAT ? r.material : r.family));
  if (kind == F_COL) {
    std::vector<std::pair<String, int>> ord;
    for (const char *f : COLOR_FAMILIES)
      for (auto &p : vals)
        if (p.first == f) ord.push_back(p);
    vals = ord;
  } else {
    std::sort(vals.begin(), vals.end(), [](const std::pair<String, int> &a, const std::pair<String, int> &b) { return a.first < b.first; });
  }
  i_picker = lv_obj_create(lv_layer_top());
  plain(i_picker, C_BG);
  lv_obj_set_style_bg_opa(i_picker, LV_OPA_90, 0);
  lv_obj_set_size(i_picker, SCR_W, 720);
  lv_obj_set_pos(i_picker, 0, 0);
  lv_obj_remove_flag(i_picker, LV_OBJ_FLAG_SCROLLABLE);
  const char *title = kind == F_BRAND ? "Filtrer par marque" : (kind == F_MAT ? "Filtrer par matière" : "Filtrer par couleur");
  lv_obj_set_pos(mk_label(i_picker, title, FB20, C_TEXT), 40, 24);
  lv_obj_set_pos(mk_button(i_picker, "Fermer", 160, 52, C_CARD2, C_TEXT, F18, ev_picker_close), 1080, 16);
  lv_obj_t *area = lv_obj_create(i_picker);
  plain(area, C_BG);
  lv_obj_set_size(area, 1220, 600);
  lv_obj_set_pos(area, 30, 84);
  lv_obj_set_scroll_dir(area, LV_DIR_VER);
  g_opts.clear();
  g_opts.push_back({kind, ""});
  for (auto &p : vals) g_opts.push_back({kind, p.first});
  const String &cur = filter_ref(kind);
  for (int i = 0; i < (int)g_opts.size(); i++) {
    String lbl;
    if (i == 0) lbl = "Tous (" + String((int)rows.size()) + ")";
    else lbl = vals[i - 1].first + " (" + String(vals[i - 1].second) + ")";
    bool on = g_opts[i].value == cur;
    lv_obj_t *b = mk_button(area, lbl.c_str(), 290, 56, on ? C_ACC : C_CARD2, on ? C_ON_ACC : C_TEXT, F18, ev_pick, (void *)(intptr_t)i);
    lv_obj_set_pos(b, (i % 4) * 300, (i / 4) * 66);
  }
}
static void ev_filter_brand(lv_event_t *) { picker_open(F_BRAND); }
static void ev_filter_mat(lv_event_t *) { picker_open(F_MAT); }
static void ev_filter_col(lv_event_t *) { picker_open(F_COL); }

static void set_filter_btn(lv_obj_t *btn, const char *name, const String &v) {
  String t = String(name) + " : " + (v.length() ? v : String("tous"));
  lv_label_set_text(lv_obj_get_child(btn, 0), t.c_str());
  lv_obj_set_style_bg_color(btn, col(v.length() ? C_ACC : C_CARD2), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(btn, 0), col(v.length() ? C_ON_ACC : C_TEXT), 0);
}

static void rebuild_inventory() {
  diag_mark("ui:inv_rebuild");
  lv_obj_clean(i_list);
  std::vector<Row> rows;
  load_rows(rows);
  std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
    if (a.material != b.material) return a.material < b.material;
    if (a.brand != b.brand) return a.brand < b.brand;
    return a.s.color_name < b.s.color_name;
  });

  int shown = 0;
  long total_g = 0;
  int y = 0;
  for (auto &r : rows) {
    if (f_brand.length() && r.brand != f_brand) continue;
    if (f_mat.length() && r.material != f_mat) continue;
    if (f_col.length() && r.family != f_col) continue;
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
    String sub = (r.s.color_name.length() ? r.s.color_name : String("-")) + "  ·  " + r.family;
    lv_obj_t *cn = mk_label(row, sub.c_str(), F16, C_MUTED);
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
  set_filter_btn(i_fb_brand, "Marque", f_brand);
  set_filter_btn(i_fb_mat, "Matière", f_mat);
  set_filter_btn(i_fb_col, "Couleur", f_col);
  if (shown == 0) {
    lv_label_set_text(i_empty, !tiger_signed_in() ? "Compte Tiger Studio non connecté." : (rows.empty() ? "Aucune bobine dans l'inventaire." : "Aucune bobine ne correspond aux filtres."));
    lv_obj_remove_flag(i_empty, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(i_empty, LV_OBJ_FLAG_HIDDEN);
  }
}

static void inv_tick(lv_timer_t *) {
  bool on = lv_screen_active() == scr_inventory;
  tiger_set_fast(on);
  if (!on) return;
  if (tiger_data_revision() != i_shown_rev || i_dirty) {
    i_shown_rev = tiger_data_revision();
    i_dirty = false;
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
  lv_obj_set_pos(mk_button(p.body, "Synchroniser", 170, 44, C_CARD2, C_TEXT, F18, ev_inv_sync), 934, 8);
  lv_obj_set_pos(mk_button(p.body, "Compte", 130, 44, C_CARD2, C_TEXT, F18, ev_open_tiger), 1114, 8);
  i_fb_brand = mk_button(p.body, "Marque : tous", 300, 44, C_CARD2, C_TEXT, F18, ev_filter_brand);
  lv_obj_set_pos(i_fb_brand, 20, 60);
  i_fb_mat = mk_button(p.body, "Matière : tous", 300, 44, C_CARD2, C_TEXT, F18, ev_filter_mat);
  lv_obj_set_pos(i_fb_mat, 332, 60);
  i_fb_col = mk_button(p.body, "Couleur : tous", 300, 44, C_CARD2, C_TEXT, F18, ev_filter_col);
  lv_obj_set_pos(i_fb_col, 644, 60);
  lv_obj_set_pos(mk_button(p.body, "Réinitialiser", 200, 44, C_CARD2, C_MUTED, F18, ev_reset_filters), 956, 60);

  i_list = lv_obj_create(p.body);
  plain(i_list, C_BG);
  lv_obj_set_size(i_list, 1240, 450);
  lv_obj_set_pos(i_list, 20, 112);
  lv_obj_add_flag(i_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(i_list, LV_DIR_VER);

  i_empty = mk_label(p.body, "", F20, C_MUTED);
  lv_obj_set_pos(i_empty, 40, 160);
  lv_obj_add_flag(i_empty, LV_OBJ_FLAG_HIDDEN);
}

void ui_tiger_build() {
  build_tiger();
  build_inventory();
  lv_timer_create(tiger_tick, 1000, nullptr);
  lv_timer_create(inv_tick, 1000, nullptr);
}
