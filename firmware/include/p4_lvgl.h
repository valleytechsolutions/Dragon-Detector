#pragma once

#include <lvgl.h>
#include "p4_menu.h"
#include "p4_pointer.h"

namespace p4ui {

template<class Button>
lv_obj_t *menuButton(lv_obj_t *parent, const MenuItem &item, Button button,
                     const lv_font_t *fallback) {
  auto *b = button(parent, item.label, item.x, item.y, item.w, item.h, int(item.action));
  auto *t = lv_obj_get_child(b, 0);
  lv_obj_update_layout(b);
  const lv_font_t *font = lv_obj_get_style_text_font(t, LV_PART_MAIN);
  const lv_coord_t spacing = lv_obj_get_style_text_letter_space(t, LV_PART_MAIN);
  if (lv_txt_get_width(item.label, strlen(item.label), font, spacing, LV_TEXT_FLAG_NONE)
      > lv_obj_get_width(t) || lv_font_get_line_height(font) > lv_obj_get_content_height(b)) {
    lv_obj_set_style_text_font(t, fallback, LV_PART_MAIN);
  }
  lv_label_set_long_mode(t, LV_LABEL_LONG_CLIP);
  return b;
}

template<class Button, size_t Count>
void menuRows(lv_obj_t *parent, const MenuItem (&items)[Count], Button button,
              const lv_font_t *fallback) {
  for (const auto &item : items) menuButton(parent, item, button, fallback);
}

// Called after the board maps touch coordinates into display pixels. A short
// edge tap is replayed to LVGL in order. Recognized swipes emit no press.
class PointerReader {
 public:
  explicit PointerReader(int diameter) : filter_(diameter) {}
  template<class Read, class Notify>
  void read(lv_indev_data_t *data, Read readPhysical, Notify notify, uint32_t now,
            bool enableEdges) {
    if (next_ == batch_.count) {
      const PointerFrame raw = readPhysical();
      batch_ = filter_.sample(raw.pressed, raw.at, now, enableEdges);
      next_ = 0;
      if (batch_.gesture != Gesture::None) notify(batch_.gesture);
    }
    if (next_ < batch_.count) {
      const PointerFrame event = batch_.frames[next_++];
      data->state = event.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
      last_ = event.at;
    } else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = lv_coord_t(last_.x);
    data->point.y = lv_coord_t(last_.y);
    data->continue_reading = next_ < batch_.count;
  }
 private:
  EdgePointer filter_;
  PointerBatch batch_ = {};
  unsigned next_ = 0;
  Point last_ = {};
};

}  // namespace p4ui
