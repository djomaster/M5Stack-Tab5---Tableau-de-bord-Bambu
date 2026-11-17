#include "display_port.h"
#include <M5Unified.h>
#include <lvgl.h>
#include "esp_heap_caps.h"

static uint32_t tick_cb() {
  return millis();
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const int32_t w = lv_area_get_width(area);
  const int32_t h = lv_area_get_height(area);
  M5.Display.startWrite();
  M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<lgfx::rgb565_t *>(px_map));
  M5.Display.endWrite();
  lv_display_flush_ready(disp);
}

static void touch_cb(lv_indev_t *, lv_indev_data_t *data) {
  static int32_t last_x = 0;
  static int32_t last_y = 0;
  if (M5.Touch.getCount() > 0) {
    auto t = M5.Touch.getDetail(0);
    last_x = t.x;
    last_y = t.y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
  data->point.x = last_x;
  data->point.y = last_y;
}

void display_port_init() {
  // L'écran du Tab5 est monté en portrait : on le met en paysage (1280 x 720).
  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation(1);
  }
  M5.Display.setBrightness(180);
  M5.Display.fillScreen(TFT_BLACK);

  const int32_t w = M5.Display.width();
  const int32_t h = M5.Display.height();
  Serial.printf("Ecran LVGL : %d x %d\n", (int)w, (int)h);

  lv_init();
  lv_tick_set_cb(tick_cb);

  lv_display_t *disp = lv_display_create(w, h);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

  const size_t lines = 60;
  const size_t bytes = (size_t)w * lines * 2;
  void *buf1 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  void *buf2 = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!buf1 || !buf2) {
    Serial.println("ERREUR : memoire insuffisante pour les tampons d'affichage");
    while (true) delay(1000);
  }
  lv_display_set_buffers(disp, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush_cb);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);
}
