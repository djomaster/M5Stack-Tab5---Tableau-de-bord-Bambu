// Écran « Imprimantes » (liste, suppression) et assistant d'ajout en 3 étapes :
//   1. Recherche sur le réseau (ou saisie manuelle)   2. Adresse, numéro de série, code d'accès + test de connexion   3. Nom
#include <lvgl.h>
#include <vector>
#include "bambu.h"
#include "discovery.h"
#include "fonts.h"
#include "printers.h"
#include "ui_common.h"

// =====================================================================
//  Liste des imprimantes
// =====================================================================
static lv_obj_t *list_body;
static std::vector<lv_obj_t *> row_status;
static int pending_del = -1;

static void ev_back_to_settings_from_list(lv_event_t *e) {
  pending_del = -1;
  ev_go_settings(e);
}

static void ev_add(lv_event_t *) { wizard_open(); }

static void ev_delete(lv_event_t *e) {
  lv_obj_t *b = (lv_obj_t *)lv_event_get_target(e);
  int idx = (int)(intptr_t)lv_obj_get_user_data(b);
  if (pending_del != idx) {
    pending_del = idx;
    lv_label_set_text(lv_obj_get_child(b, 0), "Confirmer ?");
    lv_obj_set_style_bg_color(b, col(C_WARN), 0);
    return;
  }
  pending_del = -1;
  printers_remove(idx);
  bambu_reload();
  toast("Imprimante supprimée");
  printers_screen_open();
}

void printers_screen_open() {
  pending_del = -1;
  lv_obj_clean(list_body);
  row_status.clear();
  int n = printers_count();
  if (n == 0) {
    lv_obj_set_pos(mk_label(list_body, "Aucune imprimante enregistrée.", F20, C_MUTED), 140, 30);
  }
  for (int i = 0; i < n; i++) {
    const PrinterCfg &c = printers_get(i);
    lv_obj_t *card = mk_card(list_body, 1000, 90, C_LINE);
    lv_obj_set_pos(card, 140, 24 + i * 104);
    lv_obj_set_pos(mk_label(card, c.name.c_str(), FB20, C_TEXT), 24, 12);
    String info = c.ip + "   |   " + c.serial;
    lv_obj_set_pos(mk_label(card, info.c_str(), F16, C_MUTED), 24, 50);
    lv_obj_t *st = mk_label(card, "", F18, C_MUTED);
    lv_obj_set_width(st, 260);
    lv_obj_set_style_text_align(st, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(st, 440, 30);
    row_status.push_back(st);
    lv_obj_t *del = mk_button(card, "Supprimer", 170, 52, C_CARD2, C_TEXT, F18, ev_delete);
    lv_obj_set_user_data(del, (void *)(intptr_t)i);
    lv_obj_set_pos(del, 790, 16);
  }
  int y = 24 + n * 104 + 10;
  if (n < MAX_PRINTERS) {
    lv_obj_t *b = mk_button(list_body, "Ajouter une imprimante", 420, 64, C_ACC, C_ON_ACC, FB20, ev_add);
    lv_obj_set_pos(b, 140, y);
  } else {
    lv_obj_set_pos(mk_label(list_body, "Maximum atteint : 3 imprimantes.", F18, C_MUTED), 140, y + 8);
  }
  go(scr_printers);
}

static void list_tick(lv_timer_t *) {
  for (size_t i = 0; i < row_status.size(); i++) {
    PrinterState s;
    if (!bambu_state((int)i, s)) continue;
    lv_label_set_text(row_status[i], link_text((int)s.link, s.gcode.c_str(), s.got_report, s.error));
  }
}

// =====================================================================
//  Assistant d'ajout
// =====================================================================
static lv_obj_t *wz_step[3];
static lv_obj_t *wz_kbbox, *wz_head, *wz_disc_list, *wz_disc_hint;
static lv_obj_t *wz_ip, *wz_sn, *wz_code, *wz_name, *wz_test_status, *wz_next;
static std::vector<lv_obj_t *> wz_fields;
static int wz_cur = 0;
static int wz_found_shown = -1;
static bool wz_testing = false;
static bool wz_test_ok = false;
static String wz_model;

static String ta_text(lv_obj_t *ta) {
  String s = lv_textarea_get_text(ta);
  s.trim();
  return s;
}

static void focus_field(lv_obj_t *ta) {
  kb_set_target(ta);
  for (auto *f : wz_fields) lv_obj_set_style_border_color(f, col(f == ta ? C_ACC : C_LINE2), 0);
}

static void ev_field(lv_event_t *e) { focus_field((lv_obj_t *)lv_event_get_target(e)); }

static lv_obj_t *mk_field(lv_obj_t *parent, const char *label, int y, const char *placeholder) {
  lv_obj_set_pos(mk_label(parent, label, F18, C_MUTED), 24, y);
  lv_obj_t *ta = lv_textarea_create(parent);
  style_ta(ta);
  lv_textarea_set_placeholder_text(ta, placeholder);
  lv_obj_set_size(ta, 440, 52);
  lv_obj_set_pos(ta, 24, y + 28);
  lv_obj_add_event_cb(ta, ev_field, LV_EVENT_CLICKED, nullptr);
  wz_fields.push_back(ta);
  return ta;
}

static void show_step(int n) {
  wz_cur = n;
  for (int i = 0; i < 3; i++) {
    if (i == n) lv_obj_remove_flag(wz_step[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(wz_step[i], LV_OBJ_FLAG_HIDDEN);
  }
  if (n == 0) lv_obj_add_flag(wz_kbbox, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_remove_flag(wz_kbbox, LV_OBJ_FLAG_HIDDEN);
  static const char *titles[3] = {"Étape 1 sur 3 : recherche de l'imprimante", "Étape 2 sur 3 : identifiants et test",
                                  "Étape 3 sur 3 : nom de l'imprimante"};
  lv_label_set_text(wz_head, titles[n]);
  if (n == 0) discovery_start();
  else discovery_stop();
  if (n == 1) focus_field(wz_ip);
  if (n == 2) focus_field(wz_name);
}

static void leave_wizard() {
  discovery_stop();
  bambu_test_stop();
  wz_testing = false;
  printers_screen_open();
}

static void ev_wz_back(lv_event_t *) {
  if (wz_cur == 0) {
    leave_wizard();
    return;
  }
  if (wz_cur == 1) bambu_test_stop();
  wz_testing = false;
  show_step(wz_cur - 1);
}

static void ev_pick_found(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t *)lv_event_get_target(e));
  if (idx >= discovery_count()) return;
  Found f = discovery_get(idx);
  lv_textarea_set_text(wz_ip, f.ip.c_str());
  lv_textarea_set_text(wz_sn, f.serial.c_str());
  lv_textarea_set_text(wz_code, "");
  wz_model = f.model;
  wz_test_ok = false;
  lv_label_set_text(wz_test_status, "");
  lv_obj_add_state(wz_next, LV_STATE_DISABLED);
  show_step(1);
  focus_field(wz_code);
}

static void ev_manual(lv_event_t *) {
  lv_textarea_set_text(wz_ip, "");
  lv_textarea_set_text(wz_sn, "");
  lv_textarea_set_text(wz_code, "");
  wz_model = "";
  wz_test_ok = false;
  lv_label_set_text(wz_test_status, "");
  lv_obj_add_state(wz_next, LV_STATE_DISABLED);
  show_step(1);
}

static void refresh_found_list() {
  int n = discovery_count();
  if (n == wz_found_shown) return;
  wz_found_shown = n;
  lv_obj_clean(wz_disc_list);
  for (int i = 0; i < n; i++) {
    Found f = discovery_get(i);
    String t = f.model + "   " + f.ip + "   " + f.serial;
    lv_obj_t *b = mk_button(wz_disc_list, t.c_str(), 780, 64, C_CARD2, C_TEXT, F18, ev_pick_found);
    lv_obj_set_user_data(b, (void *)(intptr_t)i);
    lv_obj_set_pos(b, 0, i * 72);
    lv_obj_align(lv_obj_get_child(b, 0), LV_ALIGN_LEFT_MID, 16, 0);
  }
  lv_label_set_text(wz_disc_hint, n ? "Touchez votre imprimante :" : "Recherche en cours... (peut prendre une dizaine de secondes)");
}

static void ev_test(lv_event_t *) {
  PrinterCfg c;
  c.ip = ta_text(wz_ip);
  c.serial = ta_text(wz_sn);
  c.serial.toUpperCase();
  c.code = ta_text(wz_code);
  c.name = "test";
  if (c.ip.isEmpty() || c.serial.isEmpty() || c.code.isEmpty()) {
    toast("Remplissez les trois champs");
    return;
  }
  wz_test_ok = false;
  wz_testing = true;
  lv_obj_add_state(wz_next, LV_STATE_DISABLED);
  lv_obj_set_style_text_color(wz_test_status, col(C_MUTED), 0);
  lv_label_set_text(wz_test_status, "Connexion en cours...");
  bambu_test_start(c);
}

static void ev_next(lv_event_t *) {
  if (!wz_test_ok) return;
  bambu_test_stop();
  wz_testing = false;
  String def = wz_model.length() ? wz_model : String("Imprimante ") + String(printers_count() + 1);
  lv_textarea_set_text(wz_name, def.c_str());
  show_step(2);
}

static void ev_save(lv_event_t *) {
  PrinterCfg c;
  c.name = ta_text(wz_name);
  if (c.name.isEmpty()) c.name = "Imprimante " + String(printers_count() + 1);
  c.ip = ta_text(wz_ip);
  c.serial = ta_text(wz_sn);
  c.serial.toUpperCase();
  c.code = ta_text(wz_code);
  if (!printers_add(c)) {
    toast("Imprimante déjà enregistrée ou maximum atteint");
    return;
  }
  discovery_stop();
  bambu_test_stop();
  wz_testing = false;
  bambu_reload();
  toast("Imprimante ajoutée");
  printers_screen_open();
}

static void wz_tick(lv_timer_t *) {
  if (lv_screen_active() != scr_wizard) return;
  if (wz_cur == 0) {
    discovery_poll();
    refresh_found_list();
  }
  if (wz_testing) {
    PrinterState s;
    if (!bambu_test_status(s)) return;
    if (s.got_report) {
      wz_testing = false;
      wz_test_ok = true;
      lv_obj_set_style_text_color(wz_test_status, col(C_ACC_T), 0);
      lv_label_set_text(wz_test_status, "Connexion réussie. Les données de l'imprimante arrivent.");
      lv_obj_remove_state(wz_next, LV_STATE_DISABLED);
    } else if (s.link == Link::AuthFailed) {
      wz_testing = false;
      lv_obj_set_style_text_color(wz_test_status, col(C_ERR), 0);
      lv_label_set_text(wz_test_status, "Code d'accès refusé. Vérifiez le code LAN sur l'écran de l'imprimante.");
    } else if (s.link == Link::Unreachable) {
      wz_testing = false;
      lv_obj_set_style_text_color(wz_test_status, col(C_ERR), 0);
      lv_label_set_text(wz_test_status,
                        "Imprimante injoignable. Vérifiez l'adresse, le numéro de série et que le mode LAN est activé.");
    } else if (s.link == Link::Connected) {
      lv_label_set_text(wz_test_status, "Connecté, attente des données...");
    }
  }
}

void wizard_open() {
  for (auto *f : wz_fields) lv_textarea_set_text(f, "");
  wz_model = "";
  wz_test_ok = false;
  wz_testing = false;
  wz_found_shown = -1;
  lv_label_set_text(wz_test_status, "");
  lv_obj_add_state(wz_next, LV_STATE_DISABLED);
  lv_obj_clean(wz_disc_list);
  lv_label_set_text(wz_disc_hint, "Recherche en cours... (peut prendre une dizaine de secondes)");
  show_step(0);
  go(scr_wizard);
}

static lv_obj_t *mk_step(lv_obj_t *body, int w) {
  lv_obj_t *s = lv_obj_create(body);
  plain(s, C_BG);
  lv_obj_set_style_bg_opa(s, LV_OPA_TRANSP, 0);
  lv_obj_set_size(s, w, 520);
  lv_obj_set_pos(s, 0, 48);
  return s;
}

static void build_wizard() {
  Page p = make_page("Ajouter une imprimante", ev_wz_back, false);
  scr_wizard = p.scr;
  wz_head = mk_label(p.body, "", FB20, C_ACC_T);
  lv_obj_set_pos(wz_head, 24, 12);

  // Étape 1 : recherche
  wz_step[0] = mk_step(p.body, SCR_W);
  wz_disc_hint = mk_label(wz_step[0], "", F18, C_MUTED);
  lv_obj_set_pos(wz_disc_hint, 24, 4);
  wz_disc_list = lv_obj_create(wz_step[0]);
  plain(wz_disc_list, C_BG);
  lv_obj_set_style_bg_opa(wz_disc_list, LV_OPA_TRANSP, 0);
  lv_obj_set_size(wz_disc_list, 800, 330);
  lv_obj_set_pos(wz_disc_list, 24, 40);
  lv_obj_t *man = mk_button(wz_step[0], "Saisie manuelle", 320, 60, C_CARD2, C_TEXT, FB20, ev_manual);
  lv_obj_set_pos(man, 24, 400);
  lv_obj_set_pos(mk_label(wz_step[0], "L'imprimante doit être allumée, sur le même réseau WiFi, avec le mode LAN activé.", F16, C_MUTED), 24, 474);

  // Étape 2 : identifiants + test
  wz_step[1] = mk_step(p.body, 490);
  wz_ip = mk_field(wz_step[1], "Adresse IP", 0, "192.168.1.50");
  wz_sn = mk_field(wz_step[1], "Numéro de série", 92, "ex. 00M09A...");
  wz_code = mk_field(wz_step[1], "Code d'accès LAN (8 caractères)", 184, "code affiché sur l'imprimante");
  lv_obj_t *tst = mk_button(wz_step[1], "Tester la connexion", 440, 56, C_CARD2, C_TEXT, FB20, ev_test);
  lv_obj_set_pos(tst, 24, 292);
  wz_test_status = mk_label(wz_step[1], "", F18, C_MUTED);
  lv_obj_set_width(wz_test_status, 440);
  lv_label_set_long_mode(wz_test_status, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(wz_test_status, 24, 360);
  wz_next = mk_button(wz_step[1], "Suivant", 440, 56, C_ACC, C_ON_ACC, FB20, ev_next);
  lv_obj_set_pos(wz_next, 24, 450);
  lv_obj_add_state(wz_next, LV_STATE_DISABLED);

  // Étape 3 : nom
  wz_step[2] = mk_step(p.body, 490);
  wz_name = mk_field(wz_step[2], "Nom de l'imprimante", 0, "ex. X1C atelier");
  lv_obj_t *sv = mk_button(wz_step[2], "Enregistrer", 440, 60, C_ACC, C_ON_ACC, FB20, ev_save);
  lv_obj_set_pos(sv, 24, 130);

  // Clavier partagé (étapes 2 et 3), créé en dernier pour rester au premier plan
  wz_kbbox = lv_obj_create(p.body);
  plain(wz_kbbox, C_BG);
  lv_obj_set_size(wz_kbbox, 740, 340);
  lv_obj_set_pos(wz_kbbox, 520, 56);
  kb_build(wz_kbbox, 0, 0);

  lv_obj_add_flag(wz_step[1], LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(wz_step[2], LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(wz_kbbox, LV_OBJ_FLAG_HIDDEN);
  lv_timer_create(wz_tick, 500, nullptr);
}

// =====================================================================
void ui_printers_build() {
  Page p = make_page("Imprimantes", ev_back_to_settings_from_list, false);
  scr_printers = p.scr;
  list_body = p.body;
  lv_timer_create(list_tick, 1000, nullptr);
  build_wizard();
}
