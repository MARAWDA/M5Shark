#include "SharkDualBoot.h"

#if defined(HAS_SCREEN) && defined(MARAUDER_V8)

#include "Display.h"
#include "SharkTheme.h"
#include "SDInterface.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_task_wdt.h>
#include <string.h>
#include <ctype.h>

extern Display display_obj;
extern SDInterface sd_obj;

namespace SharkDualBoot {
namespace {

static char g_err_buf[48] = "";

void setErr(const char* msg) {
  if (!msg) {
    g_err_buf[0] = 0;
    return;
  }
  strncpy(g_err_buf, msg, sizeof(g_err_buf) - 1);
  g_err_buf[sizeof(g_err_buf) - 1] = 0;
  Serial.printf("[DUALBOOT] ERR: %s\n", g_err_buf);
}

void paint(const char* title, const char* line, uint16_t accent = WD_AMBER) {
  TFT_eSPI& tft = display_obj.tft;
  tft.fillScreen(TFT_BLACK);
  tft.setFreeFont(NULL);
  tft.setTextSize(1);
  tft.setTextWrap(false);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(accent, TFT_BLACK);
  tft.drawString(title, tft.width() / 2, tft.height() / 2 - 48, 2);
  tft.setTextColor(WD_BONE, TFT_BLACK);
  tft.drawString(line, tft.width() / 2, tft.height() / 2 - 8, 1);
  if (g_err_buf[0]) {
    tft.setTextColor(WD_DIM, TFT_BLACK);
    tft.drawString(g_err_buf, tft.width() / 2, tft.height() / 2 + 22, 1);
  }
  tft.setTextDatum(TL_DATUM);
}

void holdBusIdle() {
  // Separate HSPI SD bus: do not pinMode/reclaim TFT or touch pins.
  // Only idle their CS lines if already configured as outputs by TFT_eSPI.
#if defined(TFT_CS)
  digitalWrite(TFT_CS, HIGH);
#endif
#if defined(TOUCH_CS) && (TOUCH_CS >= 0)
  digitalWrite(TOUCH_CS, HIGH);
#endif
}

bool pathExists(const char* path) {
  holdBusIdle();
  if (SD.exists(path)) return true;
  char alt[96];
  size_t n = strlen(path);
  if (n >= sizeof(alt)) return false;
  for (size_t i = 0; i <= n; i++) alt[i] = (char)toupper((unsigned char)path[i]);
  if (SD.exists(alt)) return true;
  for (size_t i = 0; i <= n; i++) alt[i] = (char)tolower((unsigned char)path[i]);
  return SD.exists(alt);
}

bool isDir(const char* path) {
  holdBusIdle();
  File f = SD.open(path);
  if (!f) return false;
  bool d = f.isDirectory();
  f.close();
  return d;
}

bool mkdirOne(const char* dir) {
  holdBusIdle();
  if (pathExists(dir)) {
    if (isDir(dir)) return true;
    String bak = String(dir) + ".old";
    SD.remove(bak.c_str());
    if (!SD.rename(dir, bak.c_str()) && !SD.remove(dir)) return false;
  }
  holdBusIdle();
  if (SD.mkdir(dir)) return true;
  return isDir(dir);
}

bool mkdirParents(const char* filepath) {
  String p(filepath);
  if (!p.startsWith("/")) p = "/" + p;
  int start = 1;
  while (true) {
    int slash = p.indexOf('/', start);
    if (slash < 0) break;
    String dir = p.substring(0, slash);
    if (dir.length() > 1 && !mkdirOne(dir.c_str())) return false;
    start = slash + 1;
  }
  return true;
}

String findExisting(const char* const* paths, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (pathExists(paths[i])) return String(paths[i]);
  }
  return String();
}

const esp_partition_t* runningApp() { return esp_ota_get_running_partition(); }

bool appImageLength(const esp_partition_t* part, size_t* out_len) {
  if (!part || !out_len) return false;
  uint8_t hdr[24];
  if (esp_partition_read(part, 0, hdr, sizeof(hdr)) != ESP_OK) return false;
  if (hdr[0] != 0xE9) return false;
  const uint8_t segs = hdr[1];
  if (segs == 0 || segs > 16) return false;
  size_t pos = 24;
  for (uint8_t i = 0; i < segs; i++) {
    uint8_t sh[8];
    if (pos + 8 > part->size) return false;
    if (esp_partition_read(part, pos, sh, 8) != ESP_OK) return false;
    uint32_t size = (uint32_t)sh[4] | ((uint32_t)sh[5] << 8) |
                    ((uint32_t)sh[6] << 16) | ((uint32_t)sh[7] << 24);
    if (size == 0 || size > part->size) return false;
    pos += 8u + (size_t)size;
    if (pos > part->size) return false;
  }
  size_t end = (pos + 15u) & ~((size_t)15u);
  end += 1u;
  if (end < 256 || end > part->size) {
    if (pos >= 256 && pos <= part->size) {
      *out_len = pos;
      return true;
    }
    return false;
  }
  *out_len = end;
  return true;
}

bool openAppFile(const char* path, File* out) {
  holdBusIdle();
  File f = SD.open(path, FILE_READ);
  if (!f) {
    char alt[96];
    size_t n = strlen(path);
    if (n < sizeof(alt)) {
      for (size_t i = 0; i <= n; i++) alt[i] = (char)toupper((unsigned char)path[i]);
      f = SD.open(alt, FILE_READ);
      if (!f) {
        for (size_t i = 0; i <= n; i++) alt[i] = (char)tolower((unsigned char)path[i]);
        f = SD.open(alt, FILE_READ);
      }
    }
  }
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return false;
  }
  *out = f;
  return true;
}

bool sdFileLooksLikeApp(const char* path, size_t* out_sz = nullptr) {
  File f;
  if (!openAppFile(path, &f)) return false;
  size_t sz = f.size();
  uint8_t magic = 0;
  int nr = f.read(&magic, 1);
  f.close();
  if (sz < 128 || nr != 1 || magic != 0xE9) return false;
  if (out_sz) *out_sz = sz;
  return true;
}

bool prepareSd(bool force_remount = false) {
  holdBusIdle();
  // Boot already mounted the card (see [SD] OK logs). Trust that flag.
  if (!force_remount && sd_obj.supported) {
    // Quick sanity: can we list root? If not, remount.
    File root = SD.open("/");
    if (root) {
      root.close();
      Serial.println(F("[DUALBOOT] using boot SD mount"));
      return true;
    }
    Serial.println(F("[DUALBOOT] boot flag set but / unreadable — remount"));
    sd_obj.supported = false;
  }
  if (!force_remount && sd_obj.supported) {
    return true;
  }
  Serial.println(F("[DUALBOOT] SD not marked mounted — remount"));
  if (!sd_obj.remountSD()) {
    setErr("SD mount failed");
    return false;
  }
  return sd_obj.supported;
}

void dumpRootListing() {
  holdBusIdle();
  File root = SD.open("/");
  if (!root) {
    Serial.println(F("[DUALBOOT] cannot open /"));
    return;
  }
  Serial.println(F("[DUALBOOT] SD root:"));
  for (;;) {
    File e = root.openNextFile();
    if (!e) break;
    Serial.printf("  %s%s (%u)\n", e.name(), e.isDirectory() ? "/" : "", (unsigned)e.size());
    e.close();
  }
  root.close();
}

static const char* kSharkCandidates[] = {
    "/m5shark.bin",
    "/M5SHARK.BIN",
    "/update.bin",
    "/firmware/m5shark.bin",
    "/m5shark/firmware/m5shark.bin",
};

static const char* kBjornCandidates[] = {
    "/bjorn.bin",
    "/BJORN.BIN",
    "/loki.bin",
    "/LOKI.BIN",
    "/firmware/bjorn.bin",
    "/m5shark/firmware/bjorn.bin",
};

bool anyValidSharkOnSd(size_t* out_sz = nullptr) {
  for (size_t i = 0; i < sizeof(kSharkCandidates) / sizeof(kSharkCandidates[0]); i++) {
    size_t sz = 0;
    if (sdFileLooksLikeApp(kSharkCandidates[i], &sz) && sz >= 100 * 1024) {
      Serial.printf("[DUALBOOT] shark OK %s (%u)\n", kSharkCandidates[i], (unsigned)sz);
      if (out_sz) *out_sz = sz;
      return true;
    }
  }
  return false;
}

bool anyValidBjornOnSd(String* out_path = nullptr, size_t* out_sz = nullptr) {
  for (size_t i = 0; i < sizeof(kBjornCandidates) / sizeof(kBjornCandidates[0]); i++) {
    size_t sz = 0;
    if (sdFileLooksLikeApp(kBjornCandidates[i], &sz) && sz >= 100 * 1024) {
      Serial.printf("[DUALBOOT] bjorn OK %s (%u)\n", kBjornCandidates[i], (unsigned)sz);
      if (out_path) *out_path = kBjornCandidates[i];
      if (out_sz) *out_sz = sz;
      return true;
    }
  }
  return false;
}

bool freeEnoughFor(size_t need) {
  holdBusIdle();
  uint64_t total = SD.totalBytes();
  uint64_t used = SD.usedBytes();
  if (total == 0) return true;
  uint64_t freeb = (total > used) ? (total - used) : 0;
  Serial.printf("[DUALBOOT] SD free=%llu need=%u\n",
                (unsigned long long)freeb, (unsigned)need);
  return freeb >= need + 64ULL * 1024ULL;
}

bool sdWritable() {
  // Try several root names — some FAT stacks dislike leading-dot names.
  static const char* probes[] = {
      "/dbwtest.txt",
      "/DBWTEST.TXT",
      "/shark_w.tmp",
  };
  holdBusIdle();
  for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
    SD.remove(probes[i]);
    File t = SD.open(probes[i], FILE_WRITE);
    if (!t) {
      Serial.printf("[DUALBOOT] write probe fail open %s\n", probes[i]);
      continue;
    }
    size_t w = t.write((const uint8_t*)"OK", 2);
    t.flush();
    t.close();
    bool ok = (w == 2) && SD.exists(probes[i]);
    SD.remove(probes[i]);
    if (ok) {
      Serial.printf("[DUALBOOT] write probe OK via %s\n", probes[i]);
      return true;
    }
  }
  return false;
}

bool writeRunningAppTo(const char* dest, size_t len, const esp_partition_t* run) {
  holdBusIdle();
  if (SD.exists(dest)) SD.remove(dest);
  File out = SD.open(dest, FILE_WRITE);
  if (!out) {
    Serial.printf("[DUALBOOT] open write fail %s\n", dest);
    return false;
  }
  uint8_t* buf = (uint8_t*)malloc(4096);
  if (!buf) {
    out.close();
    return false;
  }

  {
    TFT_eSPI& tft = display_obj.tft;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(WD_AMBER, TFT_BLACK);
    tft.drawString("// SAVING SHARK", tft.width() / 2, tft.height() / 2 - 50, 2);
    tft.setTextColor(WD_BONE, TFT_BLACK);
    tft.drawString(dest, tft.width() / 2, tft.height() / 2 - 20, 1);
    tft.drawRoundRect(20, tft.height() / 2 + 10, tft.width() - 40, 18, 3, WD_CYAN);
  }

  size_t off = 0;
  uint8_t lastPct = 255;
  uint32_t lastUi = 0;
  bool ok = true;
  while (off < len) {
    size_t chunk = len - off;
    if (chunk > 4096) chunk = 4096;
    if (esp_partition_read(run, off, buf, chunk) != ESP_OK) {
      setErr("flash read fail");
      ok = false;
      break;
    }
    holdBusIdle();
    if (out.write(buf, chunk) != chunk) {
      setErr("SD write fail");
      ok = false;
      break;
    }
    off += chunk;
    esp_task_wdt_reset();
    yield();
    uint8_t pct = (uint8_t)(off * 100UL / (len ? len : 1));
    uint32_t now = millis();
    if (pct != lastPct && (now - lastUi) >= 250) {
      lastPct = pct;
      lastUi = now;
      TFT_eSPI& tft = display_obj.tft;
      const int16_t bx = 20, by = tft.height() / 2 + 10, bw = tft.width() - 40, bh = 18;
      holdBusIdle();
      tft.fillRect(bx + 2, by + 2, bw - 4, bh - 4, TFT_BLACK);
      tft.fillRect(bx + 2, by + 2, (int16_t)((bw - 4) * pct / 100), bh - 4, WD_CYAN);
      tft.fillRect(0, by + bh + 6, tft.width(), 18, TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(WD_BONE, TFT_BLACK);
      tft.drawString(String(pct) + "%", tft.width() / 2, by + bh + 16, 1);
    }
  }
  free(buf);
  out.flush();
  out.close();
  if (!ok) {
    SD.remove(dest);
    return false;
  }
  if (!sdFileLooksLikeApp(dest)) {
    setErr("verify failed");
    SD.remove(dest);
    return false;
  }
  Serial.printf("[DUALBOOT] saved %u -> %s\n", (unsigned)len, dest);
  return true;
}

bool copyRunningAppToSd() {
  setErr("");
  const esp_partition_t* run = runningApp();
  if (!run) {
    setErr("no running partition");
    return false;
  }
  size_t len = 0;
  if (!appImageLength(run, &len)) {
    setErr("image parse fail");
    return false;
  }
  Serial.printf("[DUALBOOT] backup len=%u\n", (unsigned)len);

  // Prefer live mount; only remount if needed.
  if (!prepareSd(false)) return false;
  if (!sdWritable()) {
    setErr("SD not writable");
    dumpRootListing();
    return false;
  }
  if (!freeEnoughFor(len)) {
    setErr("SD full");
    return false;
  }

  static const char* dests[] = {
      "/m5shark.bin",
      "/update.bin",
      "/firmware/m5shark.bin",
      "/m5shark/firmware/m5shark.bin",
  };
  for (size_t i = 0; i < sizeof(dests) / sizeof(dests[0]); i++) {
    const char* dest = dests[i];
    const char* slash = strrchr(dest, '/');
    if (slash && slash != dest) {
      if (!mkdirParents(dest)) {
        Serial.printf("[DUALBOOT] skip dest mkdir fail %s\n", dest);
        continue;
      }
    }
    if (writeRunningAppTo(dest, len, run)) {
      setErr("");
      return true;
    }
  }
  if (!g_err_buf[0]) setErr("all write paths fail");
  dumpRootListing();
  return false;
}

bool rebootIntoStub(const char* target) {
  Preferences prefs;
  if (!prefs.begin("dualboot", false)) {
    setErr("nvs fail");
    return false;
  }
  prefs.putString("target", target);
  prefs.end();

  const esp_partition_t* stub = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  if (!stub) {
    setErr("no stub partition");
    return false;
  }
  uint8_t magic = 0;
  if (esp_partition_read(stub, 0, &magic, 1) != ESP_OK || magic != 0xE9) {
    setErr("stub not flashed");
    return false;
  }
  if (esp_ota_set_boot_partition(stub) != ESP_OK) {
    setErr("boot stub fail");
    return false;
  }
  Serial.printf("[DUALBOOT] reboot stub target=%s\n", target);
  delay(150);
  ESP.restart();
  return true;
}

}  // namespace

bool ensureSharkBackup() {
  setErr("");
  if (!prepareSd(false)) {
    setErr("SD mount failed");
    dumpRootListing();
    return false;
  }

  dumpRootListing();

  // CRITICAL: if m5shark.bin is already on the card, never require write access.
  if (anyValidSharkOnSd()) {
    setErr("");
    return true;
  }

  Serial.println(F("[DUALBOOT] no shark.bin found — need writable SD to dump"));
  if (!sdWritable()) {
    setErr("put m5shark.bin on SD");
    paint("// BJORN CYD", "Copy m5shark.bin to SD root", WD_RED);
    delay(500);
    return false;
  }
  return copyRunningAppToSd();
}

bool launchBjorn() {
  setErr("");
  paint("// BJORN CYD", "Real firmware dual-boot...");

  // Always try a hard remount once if boot mount failed (card just reinserted).
  if (!sd_obj.supported) {
    Serial.println(F("[DUALBOOT] boot had no SD — force remount"));
    paint("// BJORN CYD", "Mounting SD...");
    sd_obj.supported = false;
    if (!sd_obj.remountSD()) {
      setErr("SD mount failed");
      paint("// BJORN CYD", "SD mount failed", WD_RED);
      TFT_eSPI& tft = display_obj.tft;
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(WD_DIM, TFT_BLACK);
      tft.drawString("Reseat SD, power cycle, retry", tft.width() / 2,
                     tft.height() / 2 + 44, 1);
      delay(3500);
      return false;
    }
  }

  if (!prepareSd(false)) {
    setErr("SD mount failed");
    paint("// BJORN CYD", "SD mount failed", WD_RED);
    delay(2500);
    return false;
  }

  paint("// BJORN CYD", "Checking Shark backup...");
  if (!ensureSharkBackup()) {
    dumpRootListing();
    if (!g_err_buf[0]) setErr("need m5shark.bin");
    // Distinguish mount/file problems
    if (!sd_obj.supported) {
      paint("// BJORN CYD", "SD not mounted", WD_RED);
    } else {
      paint("// BJORN CYD", "m5shark.bin not found/readable", WD_RED);
      TFT_eSPI& tft = display_obj.tft;
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(WD_DIM, TFT_BLACK);
      tft.drawString("Need valid m5shark.bin on SD root", tft.width() / 2,
                     tft.height() / 2 + 44, 1);
    }
    delay(4000);
    return false;
  }

  String bpath;
  size_t bsz = 0;
  if (!anyValidBjornOnSd(&bpath, &bsz)) {
    setErr("need bjorn.bin on SD");
    paint("// BJORN CYD", "Missing bjorn.bin on SD", WD_RED);
    TFT_eSPI& tft = display_obj.tft;
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(WD_DIM, TFT_BLACK);
    tft.drawString("Put bjorn.bin on SD root", tft.width() / 2, tft.height() / 2 + 40, 1);
    delay(3500);
    return false;
  }

  Serial.printf("[DUALBOOT] using bjorn %s (%u)\n", bpath.c_str(), (unsigned)bsz);
  paint("// BJORN CYD", "Rebooting dual-boot stub...");
  if (!rebootIntoStub("bjorn")) {
    paint("// BJORN CYD", "Stub missing — reflash dualboot", WD_RED);
    delay(3500);
    return false;
  }
  return true;
}

}  // namespace SharkDualBoot

#endif
