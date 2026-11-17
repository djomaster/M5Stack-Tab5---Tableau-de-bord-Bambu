#pragma once
// Verrou commun : une seule connexion TLS à la fois (poignée de main + tampons de 2 x 16 Ko en mémoire interne).
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

inline SemaphoreHandle_t tls_gate() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}

// Mémoire interne minimale (plus gros bloc libre, en octets) pour ouvrir une session TLS.
static const size_t TLS_MIN_BLOCK = 24000;

// Une session TLS (2 tampons de 16 Ko + contextes) échoue et fragmente la mémoire si le plus gros bloc est trop petit
inline bool tls_memory_ok(size_t min_block) { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= min_block; }
inline bool tls_memory_ok() { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >= TLS_MIN_BLOCK; }
