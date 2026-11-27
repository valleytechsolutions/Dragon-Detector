#include "p4_lvgl.h"
#include "p4_media_browser.h"
#include "p4_launcher.h"

static p4ui::Launcher p4Launcher;
static const p4ui::LauncherItem p4LauncherItems[]={
  {"Dragon Radar","Live nearby signals",LV_SYMBOL_GPS,1,true},
  {"Radio modes","Wi-Fi and Bluetooth listening",LV_SYMBOL_REFRESH,int(p4ui::Action::Modes),true},
  {"Radio tools","Capture, analysis and network tools",LV_SYMBOL_SETTINGS,7102,false},
  {"Biscuit Node","Search for your Biscuit core",LV_SYMBOL_WIFI,-22800,true},
  {"JCMK Node","Search for your JCMK core",LV_SYMBOL_WIFI,-22801,true},
  {"BLE Lab","Discovery and GATT inspection",LV_SYMBOL_BLUETOOTH,int(p4ui::Action::BleLab),true},
  {"Wi-Fi contacts","Nearby access points",LV_SYMBOL_WIFI,20,true},
  {"Bluetooth","Nearby Bluetooth devices",LV_SYMBOL_BLUETOOTH,int(p4ui::Action::BleContacts),true},
  {"Hunt","Follow a selected signal",LV_SYMBOL_EYE_OPEN,57,true},
  {"History","Saved detections",LV_SYMBOL_LIST,60,true},
  {"Notebook","Review observations",LV_SYMBOL_EDIT,153,true},
  {"Music","MP3 and WAV on your SD card",LV_SYMBOL_AUDIO,int(p4ui::Action::Library),true},
  {"Voice Recorder","Record to the SD card",LV_SYMBOL_AUDIO,190,true},
  {"USB Disk","Transfer SD files over USB",LV_SYMBOL_USB,7120,true},
  {"SD Files","Browse the card",LV_SYMBOL_DIRECTORY,32,true},
  {"Clock","Turn to change the face",LV_SYMBOL_BELL,133,true},
  {"Stopwatch","Timer and locksport",LV_SYMBOL_REFRESH,130,true},
  {"My Badge","Your selected badge",LV_SYMBOL_IMAGE,50,true},
  {"Alerts","Detection notifications",LV_SYMBOL_BELL,22,true},
  {"Detectors","Detection preferences",LV_SYMBOL_EYE_OPEN,61,true},
  {"Settings","Theme, sound and device",LV_SYMBOL_SETTINGS,40,true},
  {"Radio health","Coprocessor diagnostics",LV_SYMBOL_CHARGE,99,true},
  {"Activity","Live detection activity",LV_SYMBOL_LIST,116,true},
  {"SD Resources","Check the resource package",LV_SYMBOL_SD_CARD,7111,true},
  {"Sleep","Put the knob to sleep",LV_SYMBOL_EYE_CLOSE,181,true},
  {"Power off","Shut down the knob",LV_SYMBOL_POWER,180,true}
};

static p4ui::MediaView p4Scope = p4ui::MediaView::Music;
static String p4Folder = "/music";
static unsigned p4ListingPage = 0;
static p4ui::MediaListing<String> p4Listing;
static lv_obj_t *p4Info = nullptr;
static constexpr int p4FileBase = -22500;
static constexpr int p4Previous = -22510, p4Next = -22511, p4Refresh = -22512;

static bool p4CardReadable() { return sdReady && !usbDriveOn(); }

struct P4Route {
  Page page = Radar;
  int tab = 0, deviceFilter = 0;
  p4ui::MediaView scope = p4ui::MediaView::Music;
  String folder;
  unsigned listPage = 0;
  bool operator==(const P4Route &v) const {
    return page == v.page && tab == v.tab && deviceFilter == v.deviceFilter &&
      scope == v.scope && folder == v.folder && listPage == v.listPage;
  }
};
static p4ui::Trail<P4Route> p4Trail;
static bool p4Returning = false;
static p4ui::Gesture p4QueuedGesture = p4ui::Gesture::None;

static P4Route p4Route() {
  P4Route r;
  r.page = page;
  if (page == Menu) r.tab = menuTab;
  if (page == Devices) { r.deviceFilter = filter; r.listPage = devicePage; }
  if (page == Files) r.folder = directory;
  if (page == History) r.listPage = historyPage;
  if (page == P4Media) { r.scope = p4Scope; r.folder = p4Folder; r.listPage = p4ListingPage; }
  return r;
}
static void p4ObserveRoute() {
  if (page != Boot && page != Startup) p4Trail.observe(p4Route(), p4Returning);
  p4Returning = false;
}

static void p4RequestRoute(const P4Route &r, bool returning) {
  go(r.page);
  if (pending != r.page) return;
  if (r.page == Menu) menuTab = r.tab;
  if (r.page == Devices) { filter = r.deviceFilter; devicePage = int(r.listPage); }
  if (r.page == Files) directory = r.folder;
  if (r.page == History) historyPage = int(r.listPage);
  if (r.page == P4Media) {
    p4Scope = r.scope; p4Folder = r.folder; p4ListingPage = r.listPage;
  }
  p4Returning = returning;
  if (page == pending) page = Boot;  // Match ui.cpp's same-page rebuild.
}

static void p4RunGesture(bool allowed) {
  const auto gesture = p4QueuedGesture;
  p4QueuedGesture = p4ui::Gesture::None;
  if (gesture != p4ui::Gesture::SwipeUp || !allowed || page != pending) return;
  if (p4Info) { lv_msgbox_close(p4Info); return; }
  if (notice) { dismissNotice(); return; }
  P4Route r;
  if (page == Radar) {
    r.page = Menu; r.tab = 0;
    p4RequestRoute(r, false);
  } else if (p4Trail.back(r)) {
    p4RequestRoute(r, true);
  } else {
    r.page = Radar;
    p4RequestRoute(r, false);
  }
}

static void p4OpenMedia(p4ui::MediaView scope) {
  P4Route r; r.page = P4Media; r.scope = scope; r.folder = p4ui::mediaRoot(scope);
  p4RequestRoute(r, false);
}

static void p4Message(const char *caption, const char *message) {
  if (p4Info) return;
  p4Info = uiMessageBox(root, caption, message, nullptr, true);
  lv_obj_set_width(p4Info, S(260)); lv_obj_center(p4Info);
  lv_obj_add_event_cb(p4Info, [](lv_event_t*) { p4Info = nullptr; }, LV_EVENT_DELETE, nullptr);
}

static void p4BuildMedia() {
  title("MUSIC LIBRARY", p4Folder.c_str());
  auto *body = scrollBody(91, 154);
  p4Listing.load(p4Scope, p4ListingPage, [](auto visit) {
    if (!p4ui::withinMediaRoot(p4Scope, p4Folder.c_str()) || !p4CardReadable() ||
        xSemaphoreTake(sdMutex, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    if (!p4CardReadable()) { xSemaphoreGive(sdMutex); return false; }
    File folder = CardFS.open(p4Folder);
    const bool ok = folder && folder.isDirectory();
    if (ok) {
      for (File f = folder.openNextFile(); f; f = folder.openNextFile()) {
        const char *name = p4ui::entryName(p4Folder.c_str(), f.name());
        const bool more = visit(name, f.isDirectory(), (unsigned long)f.size());
        f.close();
        if (!more) break;
      }
    }
    folder.close(); xSemaphoreGive(sdMutex);
    return ok;
  });
  if (!p4Listing.available) {
    label(body, "Folder unavailable. Check the card and folder, then tap Refresh.", 6, 14, 239, F14);
    button(root,"Refresh",108,252,144,38,p4Refresh);
    return;
  }
  if (!p4Listing.count) label(body, p4ListingPage ? "This page is empty." :
      (p4Scope == p4ui::MediaView::Music ? "Copy MP3 or WAV files to /music." :
      "Copy MP3 or WAV files to /audio."), 6, 14, 239, F14);
  for (unsigned i = 0; i < p4Listing.count; ++i) {
    const auto &entry = p4Listing.entries[i];
    const char *detail = entry.kind == p4ui::MediaKind::Folder ? "Folder" :
        ("Play audio");
    row(body, entry.name.c_str(), detail, int(i), p4FileBase - int(i));
  }
  if (p4ListingPage) button(root,"Previous",63,252,112,38,p4Previous);
  if (p4Listing.hasNext) button(root,"Next",185,252,112,38,p4Next);
}

#include "p4_radio_views.inc"

static bool p4BuildPage() {
  if(page==Menu){
    p4Launcher.create(root,uiColors(),p4LauncherItems,sizeof(p4LauncherItems)/sizeof(*p4LauncherItems),[](int id){
      if(!p4Launcher.currentItem().available){p4Message("PORT IN PROGRESS","This mode is not operational in this development build.");return;}
      runAction(id);
    });
    return true;
  }
  if(p4BuildRadioPage())return true;
  if (page == P4ActivityHub) {
    title("ACTIVITY");
    p4ui::menuRows(root, p4ui::activity, button, F12);
  } else if (page == P4Library) {
    title("MUSIC LIBRARY", "Choose an SD folder");
    button(root,"/music",76,123,208,44,int(p4ui::Action::MusicFiles));
    button(root,"/audio",76,184,208,44,int(p4ui::Action::AudioFiles));
  } else if (page == P4Media) {
    p4BuildMedia();
  } else return false;
  return true;
}

struct P4ActionBindings {
  void openActivityHub() { go(P4ActivityHub); }
  void openLibrary() { go(P4Library); }
  void openChannels() { p4ChannelBank=0;go(P4Channels); }
  void openBeacons() { go(P4Beacons); }
  void openModes() { go(P4Modes); }
  void openBleLab() { go(P4BleLab); }
  void openBleContacts() { filter=2;devicePage=0;go(Devices); }
  void openRecorder() { go(Recorder); }
  void browseMedia(p4ui::MediaView scope) { p4OpenMedia(scope); }
  void openRadioActivity() { go(Activity); }
  void openHistory() { historyPage = 0; go(History); }
};

static bool p4Action(int id) {
  if(id==-22800||id==-22801){
    const bool biscuit=id==-22800;
    if(nodeStart(biscuit?NodeKind::Biscuit:NodeKind::Jcmk))go(Node);
    else p4Message(biscuit?"BISCUIT NODE":"JCMK NODE",nodeError());
    return true;
  }
  if(p4RadioAction(id))return true;
  P4ActionBindings routes;
  if (p4ui::dispatch(id, routes)) return true;
  if (page != P4Media) return false;
  auto next = p4Route();
  if (id == p4Previous && p4ListingPage) --next.listPage;
  else if (id == p4Next && p4Listing.hasNext) ++next.listPage;
  else if (id == p4Refresh) {}
  else if (id <= p4FileBase && id > p4FileBase - int(p4Listing.count)) {
    const auto &entry = p4Listing.entries[p4FileBase - id];
    const String path = p4Folder + "/" + entry.name;
    if (!p4ui::withinMediaRoot(p4Scope, path.c_str())) return true;
    if (!p4CardReadable()) { p4Message("SD CARD", "Close USB drive mode and check the card."); return true; }
    if (entry.kind == p4ui::MediaKind::Folder) { next.folder = path; next.listPage = 0; }
    else if (entry.kind == p4ui::MediaKind::Audio) {
      voiceOpen(path,P4Media);
      return true;
    }
  } else return false;
  p4RequestRoute(next, false);
  return true;
}
