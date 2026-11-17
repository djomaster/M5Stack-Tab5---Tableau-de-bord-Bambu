#include "diag.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

struct Mark {
  const char *what;
  uint32_t ms;
};
static const int N = 12;
static Mark g_ring[N];
static volatile int g_head = 0;

void diag_mark(const char *what) {
  int h = g_head;
  g_ring[h % N] = {what, millis()};
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
