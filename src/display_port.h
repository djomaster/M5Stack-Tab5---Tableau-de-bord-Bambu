#pragma once

// Relie LVGL à l'écran et au tactile du Tab5 (via M5Unified / M5GFX).
// À appeler après M5.begin() et avant fonts_init() / ui_init().
void display_port_init();
