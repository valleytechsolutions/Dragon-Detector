#include "theme_button.h"
static void themeButtonTests(){
  dragon::ThemeButton b;
  assert(!b.update(true,0));assert(!b.update(true,1000)); // held at startup
  assert(!b.update(false,1010));assert(!b.update(false,1045));
  assert(!b.update(true,1100));assert(!b.update(false,1104));
  assert(!b.update(true,1110));assert(!b.update(true,1144));
  assert(b.update(true,1145));assert(!b.update(true,60000));
  assert(!b.update(false,60010));assert(!b.update(false,60045));
  assert(!b.update(true,60100));assert(b.update(true,60135));
  dragon::ThemeButton wrap;
  assert(!wrap.update(false,0xffffffc0));assert(!wrap.update(false,0xffffffe3));
  assert(!wrap.update(true,0xfffffff0));assert(!wrap.update(true,18));assert(wrap.update(true,19));
  puts("PASS: BOOT debounce, startup hold suppression, single press, rearm and millis rollover");
}
