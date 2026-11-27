#pragma once

#include <string.h>

namespace p4ui {

enum class Action {
  ActivityHub = -22399, Recorder, Library, RadioActivity, History, Channels, Beacons, MusicFiles, AudioFiles, Modes, BleLab, BleContacts = -22386
};
struct MenuItem { const char *label; int x, y, w, h; Action action; };

// Coordinates use the same 360-unit grid as ui.cpp::button.
constexpr MenuItem utilitiesActivity = {"Activity", 79, 242, 202, 43, Action::ActivityHub};
constexpr MenuItem activity[] = {
  {"Voice Recorder", 70, 95, 220, 42, Action::Recorder},
  {"Music Library", 70, 145, 220, 42, Action::Library},
  {"Radio Activity", 70, 195, 220, 42, Action::RadioActivity},
  {"History", 70, 245, 220, 42, Action::History}
};

enum class MediaView { Music, Audio };
enum class MediaKind { Hidden, Folder, Audio };

inline const char *mediaRoot(MediaView view) {
  return view == MediaView::Music ? "/music" : "/audio";
}
inline bool equalExtension(const char *actual, const char *lower) {
  while (*actual && *lower) {
    char c = *actual++;
    if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
    if (c != *lower++) return false;
  }
  return !*actual && !*lower;
}
inline MediaKind classify(MediaView, const char *base, bool directory) {
  if (!base || !*base || *base == '.') return MediaKind::Hidden;
  for (const char *s = base; *s; ++s) {
    if (static_cast<unsigned char>(*s) < 32 || *s == '/' || *s == '\\' || *s == ':')
      return MediaKind::Hidden;
  }
  if (directory) return MediaKind::Folder;
  const char *suffix = strrchr(base, '.');
  if (!suffix) return MediaKind::Hidden;
  if (equalExtension(suffix,".mp3") || equalExtension(suffix,".wav")) return MediaKind::Audio;
  return MediaKind::Hidden;
}

inline bool withinMediaRoot(MediaView view, const char *path) {
  if (!path) return false;
  const char *root = mediaRoot(view);
  const size_t length = strlen(root);
  if (strncmp(path, root, length) != 0) return false;
  const char *rest = path + length;
  while (*rest) {
    if (*rest++ != '/' || !*rest || *rest == '.' || *rest == '/') return false;
    while (*rest && *rest != '/') {
      if (static_cast<unsigned char>(*rest) < 32 || *rest == '\\' || *rest == ':') return false;
      ++rest;
    }
  }
  return true;
}

template<class Ui> bool dispatch(int id, Ui &ui) {
  switch (static_cast<Action>(id)) {
    case Action::ActivityHub: ui.openActivityHub(); return true;
    case Action::Recorder: ui.openRecorder(); return true;
    case Action::Library: ui.openLibrary(); return true;
    case Action::MusicFiles: ui.browseMedia(MediaView::Music); return true;
    case Action::AudioFiles: ui.browseMedia(MediaView::Audio); return true;
    case Action::Channels: ui.openChannels(); return true;
    case Action::Beacons: ui.openBeacons(); return true;
    case Action::Modes: ui.openModes(); return true;
    case Action::BleLab: ui.openBleLab(); return true;
    case Action::BleContacts: ui.openBleContacts(); return true;
    case Action::RadioActivity: ui.openRadioActivity(); return true;
    case Action::History: ui.openHistory(); return true;
  }
  return false;
}
}  // namespace p4ui
