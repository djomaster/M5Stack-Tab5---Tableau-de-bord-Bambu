#pragma once

enum class Sound { Finish, Error, Test };

void alerts_load();                 // lit les réglages (sons activés, volume)
bool alerts_enabled();
void alerts_set_enabled(bool on);
int alerts_volume();                // 1 à 5
void alerts_set_volume(int v);
void alerts_play(Sound s);
void alerts_tick();                 // à appeler chaque seconde : détecte fin d'impression et erreurs
