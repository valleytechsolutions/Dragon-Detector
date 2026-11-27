#pragma once

#include "p4_menu.h"

namespace p4ui {

// scan(visitor) owns the card lock and each file handle. It returns false if
// the directory could not be opened. Its visitor returns false to stop early.
// Only displayed names are retained; the library can span multiple pages.
template<class Text, unsigned PageSize = 8>
struct MediaListing {
  struct Entry { Text name; MediaKind kind; unsigned long bytes; };
  Entry entries[PageSize];
  unsigned count = 0;
  bool hasNext = false;
  bool available = false;
  template<class Scan>
  void load(MediaView view, unsigned page, Scan scan) {
    count = 0;
    hasNext = false;
    unsigned index = 0;
    available = scan([&](const char *base, bool directory, unsigned long bytes) {
      const auto kind = classify(view, base, directory);
      if (kind == MediaKind::Hidden) return true;
      if (index++ / PageSize < page) return true;
      if (count == PageSize) { hasNext = true; return false; }
      entries[count++] = {Text(base), kind, bytes};
      return true;
    });
    if (!available) { count = 0; hasNext = false; }
  }
};

// Accept either the basename Arduino FS supplies or an absolute filename
// directly inside the enumerated directory from a different FS adapter.
inline const char *entryName(const char *directory, const char *reported) {
  if (!reported || *reported != '/') return reported;
  const size_t n = strlen(directory);
  if (strncmp(directory, reported, n) != 0 || reported[n] != '/') return nullptr;
  return reported + n + 1;
}
}  // namespace p4ui
