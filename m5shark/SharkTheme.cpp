#include "SharkTheme.h"

#if defined(HAS_SCREEN) && defined(MARAUDER_V8)

#include <Preferences.h>

// Colors are RGB565. Each theme keeps the same semantic slots, so a screen
// written for one theme is legible in all of them: accent carries system
// state, warn carries warnings and toggles, danger marks offensive tooling.
const SharkThemeDef shark_themes[SHARK_THEME_COUNT] = {
  // 0 - WATCH_DOGS / ctOS: bone white on black with a single cyan accent.
  {
    "Watch Dogs", "ctOS", "WATCH_DOGS",
    "SCANNING CARRIER", "DECRYPTING PROFILE", "SIGNAL BREAK", "ACCESS GRANTED",
    SHARK_SPLASH_MARK, SHARK_MARK_SKULL,
    0x07FF, 0x063A, 0x036F,
    0xFFFF, 0xC618, 0x8410, 0x39E8, 0x29C8, 0x1904, 0x10A3,
    0xFD20, 0xF967,
    0xFFFF, 0xC618, 0x07FF,
    SHARK_STYLE_CTOS
  },
  // 1 - MATRIX: phosphor green rain, everything reads as terminal output.
  {
    "Matrix", "MTRX", "WAKE UP, NEO",
    "TRACING SIGNAL", "DECODING STREAM", "AGENT DETECTED", "THE MATRIX HAS YOU",
    SHARK_SPLASH_MARK, SHARK_MARK_CYBER,
    0x07E0, 0x05E0, 0x0300,
    // High-contrast phosphor ladder for the physical TFT.  Secondary and dim
    // text previously used 0x03E0/0x0180 against a 0x0120 surface, making the
    // deepest green labels effectively disappear at normal viewing angles.
    0xAFF5, 0x07E0, 0x06E0, 0x04E0, 0x0240, 0x0120, 0x00E0,
    0xFFE0, 0xF800,
    0xAFF5, 0x07E0, 0xFFFF,
    SHARK_STYLE_TERMINAL
  },
  // 2 - CYBER2077: 2077 yellow with cyan and red chromatic aberration.
  {
    "Cyber 2077", "2077", "SAMURAI",
    "BREACH PROTOCOL", "UPLOADING DAEMON", "ICE SPIKE", "WAKE UP SAMURAI",
    SHARK_SPLASH_CHROMA, SHARK_MARK_DEMON,
    0xF7A1, 0xC5E0, 0x6B40,
    0xFFFF, 0xE73C, 0x8410, 0x39A2, 0x4A02, 0x1881, 0x1040,
    0x07FF, 0xF807,
    0xF7A1, 0xFFFF, 0x07FF,
    SHARK_STYLE_NEON
  },
  // 3 - SPIDER: red body over a deep blue chassis, web-line accents.
  {
    "Spider", "WEB", "SPIDER_NET",
    "SPINNING WEB", "TRACING THREADS", "THREAD SNAPPED", "SPIDEY SENSE ON",
    SHARK_SPLASH_MARK, SHARK_MARK_SPIDER,
    0xD904, 0xFACB, 0x7082,
    0xFFFF, 0xC618, 0x8410, 0x39E8, 0x2B5C, 0x0884, 0x0842,
    0xFD20, 0xF800,
    0xD904, 0xFACB, 0x2CDF,
    SHARK_STYLE_BLOCK
  },
  // 4 - AURORA: green-blue polar bands with quiet teal surfaces.
  {
    "Aurora", "AUR", "POLAR LIGHT",
    "SYNCING BAY", "CAPTURING AURORA", "FIELD SHIFT", "WINDOW OPEN",
    SHARK_SPLASH_AURORA, SHARK_MARK_ALIEN,
    0x05FF, 0x04D7, 0x02A5,
    0xF7FF, 0xB5FF, 0x6B7A, 0x1D7A, 0x0E2E, 0x0818, 0x0520,
    0x07FF, 0xF800,
    0x05FF, 0xB5FF, 0x07FF,
    SHARK_STYLE_TERMINAL
  },
  // 5 - CRIMSON: red-black tactical overlay with amber warning accents.
  {
    "Crimson", "BLOOD", "REDLINE",
    "BREACH TRACE", "LOCKING VECTOR", "SIGNAL BURN", "LINE SECURE",
    SHARK_SPLASH_EMBER, SHARK_MARK_DEMON,
    0xF800, 0xD000, 0x9800,
    0xFFFF, 0xF7BE, 0xAD40, 0x1D0B, 0x2008, 0x1004, 0x0C02,
    0xFD20, 0xF800,
    0xF800, 0xF7BE, 0xFD20,
    SHARK_STYLE_BLOCK
  },
  // 6 - SUNSET: lime-orange synthwave horizon with a violet base.
  {
    "Sunset", "DUSK", "SYNTHWAVE",
    "CARRIER HUSH", "PULSE WARMUP", "LANGER SIGNAL", "FUSION READY",
    SHARK_SPLASH_SUNSET, SHARK_MARK_BIO,
    0xFBE0, 0xE981, 0xAA80,
    0xFFFF, 0xF7BE, 0xF4A1, 0x1A0F, 0x280C, 0x1806, 0x0B03,
    0xFD20, 0xF800,
    0xFBE0, 0xF7BE, 0xF81F,
    SHARK_STYLE_NEON
  },
  // 7 - VIOLET: deep purple with magenta highlights and cool neon edges.
  {
    "Violet", "VLT", "GHOST WIRE",
    "STABILIZING FEED", "SHADOW LINK", "NOISE DROP", "PULSE LOCKED",
    SHARK_SPLASH_WEB, SHARK_MARK_REAPER,
    0xA7FF, 0x7BFE, 0x2B5A,
    0xFFFF, 0xD4AF, 0x8D3A, 0x2107, 0x180B, 0x0E05, 0x0903,
    0x07FF, 0xF800,
    0xA7FF, 0xD4AF, 0x07FF,
    SHARK_STYLE_SOFT
  },
  // 8 - GHOST: bone white on charcoal with a low-contrast silver edge.
  {
    "Ghost", "GHO", "SILENT MODE",
    "SWEEPING DARK", "MUTE LOOP", "PASSIVE SCAN", "NO SIGNAL LEFT",
    SHARK_SPLASH_RADAR, SHARK_MARK_NONE,
    0xC618, 0xBDF7, 0x8C71,
    0xFFFF, 0xD69A, 0x94B2, 0x2A0D, 0x1CE7, 0x1022, 0x0A12,
    0xFD20, 0xF800,
    0xF7DE, 0xD69A, 0x07FF,
    SHARK_STYLE_MINIMAL
  },
  // 9 - NEON: fluorescent lime and cyan with a hard minimal shell.
  {
    "Neon", "NXT", "HYPER DRIVE",
    "RADIANT SCAN", "HYPERSTREAM", "GLITCH EDGE", "NEON LOCK",
    SHARK_SPLASH_RAIN, SHARK_MARK_CYBER,
    0x07FF, 0x0FFE, 0x07C0,
    0xFFFF, 0xC618, 0x83E0, 0x24E9, 0x1CE7, 0x1022, 0x0A12,
    0xFD20, 0xF800,
    0x07FF, 0xAFF5, 0x07E0,
    SHARK_STYLE_MINIMAL
  },
};

static_assert(SHARK_THEME_COUNT == 10, "This release exposes ten themes");

const SharkThemeDef* shark_theme = &shark_themes[0];
uint8_t shark_theme_index = 0;

namespace {
  Preferences shark_prefs;
  bool shark_prefs_ready = false;
}

void sharkThemeBegin() {
  if (!shark_prefs_ready)
    shark_prefs_ready = shark_prefs.begin("shark_ui", false);

  // Matrix (green terminal) is the factory default theme; an explicitly
  // saved theme choice still wins.
  uint8_t stored = shark_prefs_ready ? shark_prefs.getUChar("theme", 1) : 1;
  if (stored >= SHARK_THEME_COUNT)
    stored = 1;

  shark_theme_index = stored;
  shark_theme = &shark_themes[stored];
}

void sharkThemeSet(uint8_t index) {
  if (index >= SHARK_THEME_COUNT)
    return;

  shark_theme_index = index;
  shark_theme = &shark_themes[index];

  if (!shark_prefs_ready)
    shark_prefs_ready = shark_prefs.begin("shark_ui", false);
  if (shark_prefs_ready)
    shark_prefs.putUChar("theme", index);
}

#endif
