#include "SharkSentinel.h"
#include "WiFiScan.h"
#include "SDInterface.h"
#include <Preferences.h>

extern WiFiScan wifi_scan_obj;
extern SDInterface sd_obj;

void SharkSentinel::begin() {
  last_tick_ = millis();
  Preferences prefs;
  // NVS keeps the operator's choice across reboot without writing the SD card.
  if (prefs.begin("sentinel", true)) {
    enabled_ = prefs.getBool("enabled", false);
    prefs.end();
  }
  Serial.println(F("[SENTINEL] ready"));
}

void SharkSentinel::setEnabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled_) {
    deauth_latched_ = false;
    twin_latched_ = false;
  }
  Preferences prefs;
  // Persist only the setting; alert records belong on serial/SD instead.
  if (prefs.begin("sentinel", false)) {
    prefs.putBool("enabled", enabled_);
    prefs.end();
  }
  Serial.println(enabled_ ? F("[SENTINEL] enabled") : F("[SENTINEL] disabled"));
}

void SharkSentinel::tick(uint32_t now) {
  // Avoid doing storage work on every pass through the firmware loop.
  if (!enabled_ || now - last_tick_ < 250) return;
  last_tick_ = now;

  // These signals are calculated by WiFiScan's existing passive detectors.
  const bool deauth_alert = wifi_scan_obj.deauth_alarm_active &&
                            wifi_scan_obj.deauth_alarm_rate >= 5;
  const bool twin_alert = wifi_scan_obj.evil_twin &&
                          wifi_scan_obj.evil_twin_count > 0;
  if (deauth_alert && !deauth_latched_) {
    record("deauth_rate", String(wifi_scan_obj.deauth_alarm_rate) + "/s");
  }
  if (twin_alert && !twin_latched_) {
    record("evil_twin", String(wifi_scan_obj.evil_twin_count));
  }
  deauth_latched_ = deauth_alert;
  twin_latched_ = twin_alert;
}

void SharkSentinel::record(const char* type, const String& detail) {
  // One line is easy to stream over serial and parse later from the SD log.
  ++alert_count_;
  const String line = String(millis()) + "," + type + "," + detail;
  Serial.println(String("[SENTINEL] ALERT ") + line);

#ifdef HAS_SD
  if (sd_obj.supported) {
    if (!SD.exists("/shark")) SD.mkdir("/shark");
    File log = SD.open("/shark/sentinel.log", FILE_APPEND);
    if (log) {
      log.println(line);
      log.close();
    }
  }
#endif
}

SharkSentinel shark_sentinel;
