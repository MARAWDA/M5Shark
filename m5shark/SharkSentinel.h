#pragma once

#include <Arduino.h>

class SharkSentinel {
  public:
    // Call once during setup, before the main loop starts calling tick().
    void begin();
    // Call frequently from loop(); the service internally rate-limits itself.
    void tick(uint32_t now);
    // The menu and future web controls use this to arm or disarm monitoring.
    void setEnabled(bool enabled);
    // These read-only accessors make the service state available to the UI/API.
    bool enabled() const { return enabled_; }
    uint32_t alertCount() const { return alert_count_; }

  private:
    // This is intentionally passive: it observes existing Wi-Fi detector state.
    bool enabled_ = false;
    // Latches prevent the same sustained condition from filling the log every tick.
    bool deauth_latched_ = false;
    bool twin_latched_ = false;
    // Timing is kept here so loop() remains responsive to touch and radio work.
    uint32_t last_tick_ = 0;
    uint32_t alert_count_ = 0;
    void record(const char* type, const String& detail);
};

extern SharkSentinel shark_sentinel;
