#pragma once
#include <Arduino.h>
#include <lvgl.h>

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
#define C_ON_ACC 0x06281F

static const int SCR_W = 1280;
static const int TOP_H = 64;
static const int NAV_H = 80;

struct Page {
  lv_obj_t *scr;
  lv_obj_t *body;
};

// Écrans (créés dans ui.cpp et ui_printers.cpp)
extern lv_obj_t *scr_home, *scr_settings, *scr_wifi, *scr_about, *scr_printers, *scr_wizard, *scr_detail, *scr_alerts, *scr_tiger, *scr_inventory;

lv_color_t col(uint32_t c);
void plain(lv_obj_t *o, uint32_t bg);
lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, const lv_font_t *f, uint32_t c);
lv_obj_t *mk_card(lv_obj_t *parent, int w, int h, uint32_t border);
lv_obj_t *mk_button(lv_obj_t *parent, const char *txt, int w, int h, uint32_t bg, uint32_t fg, const lv_font_t *f,
                    lv_event_cb_t cb, void *ud = nullptr);
void style_ta(lv_obj_t *ta);
void toast(const char *txt);
void go(lv_obj_t *s);

// Écran type : barre du haut (titre, état), zone centrale, barre de navigation.
// back_cb : action du bouton « Retour » (nullptr = pas de bouton).
enum { NAV_HOME = 0, NAV_INV = 1, NAV_SET = 2 };  // onglet actif de la barre de navigation
Page make_page(const char *title, lv_event_cb_t back_cb, int nav_active);
void ui_status_apply(const char *text);

void ev_go_home(lv_event_t *e);
void ev_go_settings(lv_event_t *e);

// Clavier tactile AZERTY partagé (Maj, Verr. Maj, Espace, Effacer, Vider).
// Les touches écrivent dans le champ désigné par kb_set_target().
void kb_build(lv_obj_t *parent, int x, int y);
void kb_set_target(lv_obj_t *ta);
static const int KB_Y = 200;  // le clavier occupe le bas de la page (pleine largeur) ; les champs restent au-dessus
void kb_attach(lv_obj_t *parent, int x, int y);
void kb_hide();
void kb_show();  // ré-affiche le clavier réduit (toucher un champ de saisie)  // déplace le clavier (unique) sur un autre écran

// Écrans des imprimantes (ui_printers.cpp)
void ui_printers_build();
void printers_screen_open();
void wizard_open();

// Vue détaillée et réglage des alertes (ui_detail.cpp)
void ui_detail_build();
void detail_open(int printer_index);
void alerts_screen_open();

// Tiger Studio : compte et inventaire (ui_tiger.cpp)
void ui_tiger_build();
void tiger_screen_open();
void inventory_open();

// Réglages > Affichage (ui_display.cpp)
extern lv_obj_t *scr_display;
void ui_display_build();
void display_screen_open();

// Libellés d'état partagés
const char *link_text(int link, const char *gcode, bool got_report, int error);
String fmt_remaining(int minutes);
