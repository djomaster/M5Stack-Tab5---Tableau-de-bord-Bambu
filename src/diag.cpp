#include "diag.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <string.h>

struct Mark {
  const char *what;
  uint32_t ms;
};
static const int N = 16;
// Le tampon survit à un redémarrage logiciel (panique, watchdog) : au démarrage suivant on sait ce qui se passait avant le plantage.
struct Ring {
  uint32_t magic;
  int head;
  Mark m[N];
};
static RTC_NOINIT_ATTR Ring g_r;
static const uint32_t MAGIC = 0xD1A60001u ^ (uint32_t)(sizeof(__DATE__ __TIME__)) ^ ((uint32_t)__TIME__[7] << 8) ^ ((uint32_t)__TIME__[6] << 16) ^ ((uint32_t)__TIME__[4] << 24);
#define g_ring g_r.m
#define g_head g_r.head

void diag_init() {
  if (g_r.magic != MAGIC) {
    memset(&g_r, 0, sizeof(g_r));
    g_r.magic = MAGIC;
  }
}

void diag_report_previous() {
  esp_reset_reason_t r = esp_reset_reason();
  if (r == ESP_RST_POWERON || r == ESP_RST_SW) return;
  int h = g_head;
  Serial.print("Avant le redemarrage, dernieres etapes :");
  uint32_t last = 0;
  for (int i = N; i >= 1; i--) {
    int k = h - i;
    if (k < 0) continue;
    Mark m = g_ring[k % N];
    if (m.what) {
      Serial.printf(" [%s @%ums]", m.what, (unsigned)m.ms);
      last = m.ms;
    }
  }
  Serial.printf(" (derniere a %u ms)\n", (unsigned)last);
  memset(&g_r.m, 0, sizeof(g_r.m));
  g_r.head = 0;
}

void diag_mark(const char *what) {
  int h = g_head;
  g_ring[h % N] = {what, (uint32_t)millis()};
  g_head = h + 1;
}

void diag_loop() {
  static uint32_t last_t = 0;
  static uint32_t last_free = 0;
  uint32_t now = millis();
  if (now - last_t < 200) return;
  last_t = now;
  uint32_t cur = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (last_free && last_free > cur && last_free - cur > 12000) {
    Serial.printf("Mem CHUTE : %u -> %u (bloc max %u). Dernieres etapes :", (unsigned)last_free, (unsigned)cur,
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    int h = g_head;
    for (int i = 8; i >= 1; i--) {
      int k = h - i;
      if (k < 0) continue;
      Mark m = g_ring[k % N];
      if (m.what) Serial.printf(" [%s -%ums]", m.what, (unsigned)(now - m.ms));
    }
    Serial.println();
  }
  last_free = cur;
}
