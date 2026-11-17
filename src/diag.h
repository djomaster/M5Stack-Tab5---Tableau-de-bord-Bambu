#pragma once
// Diagnostic mémoire : repères horodatés + détection des chutes brutales de la mémoire interne.
void diag_mark(const char *what);  // enregistre une étape (pointeur sur texte constant)
void diag_loop();                  // à appeler souvent : affiche les dernières étapes si la mémoire chute
