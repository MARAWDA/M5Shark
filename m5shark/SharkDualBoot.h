#pragma once
#include "configs.h"

// SD dual-boot between M5Shark and MARAWDA/cyd_bjorn-port (Loki/Bjorn CYD).
// 4 MB flash cannot hold both firmwares at once, so the inactive image lives
// on SD and is flashed into the single app slot on demand.
namespace SharkDualBoot {
  // Paths searched (first hit wins).
  static constexpr const char* BJORN_PATHS[] = {
    "/m5shark/firmware/bjorn.bin",
    "/firmware/bjorn.bin",
    "/bjorn.bin",
    "/loki.bin",
  };
  static constexpr const char* SHARK_SAVE_PATH = "/m5shark/firmware/m5shark.bin";
  static constexpr const char* BJORN_SAVE_HINT = "/m5shark/firmware/bjorn.bin";

  // Ensure /m5shark/firmware exists and snapshot the running Shark image so
  // Bjorn can restore it on Exit. Returns false if SD is missing/full.
  bool ensureSharkBackup();

  // Flash Bjorn/Loki from SD and reboot into it. Blocking UI with progress.
  // Returns only on failure (true = handed off / will restart).
  bool launchBjorn();
}
