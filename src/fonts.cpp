#include "fonts.h"
#include "assets/font_regular.h"
#include "assets/font_bold.h"

const lv_font_t *F14, *F16, *F18, *F20, *F24;
const lv_font_t *FB16, *FB18, *FB20, *FB24, *FB28;

void fonts_init() {
  F14 = lv_tiny_ttf_create_data(font_regular_ttf, font_regular_ttf_len, 14);
  F16 = lv_tiny_ttf_create_data(font_regular_ttf, font_regular_ttf_len, 16);
  F18 = lv_tiny_ttf_create_data(font_regular_ttf, font_regular_ttf_len, 18);
  F20 = lv_tiny_ttf_create_data(font_regular_ttf, font_regular_ttf_len, 20);
  F24 = lv_tiny_ttf_create_data(font_regular_ttf, font_regular_ttf_len, 24);
  FB16 = lv_tiny_ttf_create_data(font_bold_ttf, font_bold_ttf_len, 16);
  FB18 = lv_tiny_ttf_create_data(font_bold_ttf, font_bold_ttf_len, 18);
  FB20 = lv_tiny_ttf_create_data(font_bold_ttf, font_bold_ttf_len, 20);
  FB24 = lv_tiny_ttf_create_data(font_bold_ttf, font_bold_ttf_len, 24);
  FB28 = lv_tiny_ttf_create_data(font_bold_ttf, font_bold_ttf_len, 28);
}
