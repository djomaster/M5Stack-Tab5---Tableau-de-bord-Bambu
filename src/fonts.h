#pragma once
#include <lvgl.h>

// Polices Liberation Sans embarquées (accents français inclus), rendues par LVGL (TinyTTF).
// F = normal, FB = gras, suivi de la taille en pixels.
extern const lv_font_t *F14, *F16, *F18, *F20, *F24;
extern const lv_font_t *FB16, *FB18, *FB20, *FB24, *FB28;

void fonts_init();
