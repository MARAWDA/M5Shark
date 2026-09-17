#pragma once

#include <Arduino.h>
#include "configs.h"

#ifdef HAS_NRF24

// nRF24L01 toolbox for M5SHARK.
// Raw SPI register access (no RF24 library) so the module can share HSPI with SD.
// Attack UIs are ported from Evaware / ESP32-DIV style flows and use Shark theme colors.

class Nrf24Interface {
  public:
    Nrf24Interface();

    bool begin();
    bool isPresent() const;
    void reset();
    void setChannel(uint8_t channel);
    void setPowerLevel(uint8_t level);
    uint8_t readStatus() const;

    // Serial / quick checks (existing)
    void runDiagnostic();
    void runChannelScan();
    void runCarrierHeatmap();
    void runRxTest();
    void runJammerTest();

    // Evaware-style full-screen attack tools (blocking until BACK touch)
    void runSpectrumScanner();    // Scanner — RPD bar graph 0-84
    void runSpectrumAnalyzer();   // Analyzer — hopping spectrum levels
    void runWlanJammer();         // WLAN jammer — CW hop WiFi ch 1-13
    void runProtoKill();          // ProtoKill — broadband 2.4GHz CW hop
    void runPromiscuousSniffer(); // Goodspeed promiscuous address sniffer
    void runMouseJack();          // MouseJack discovery + inject demo

  private:
    bool initialized_ = false;
    bool present_ = false;
    uint8_t channel_ = 76;
    uint8_t powerLevel_ = 3;

    uint8_t readRegister(uint8_t reg) const;
    void writeRegister(uint8_t reg, uint8_t value) const;
    void writeRegisterMulti(uint8_t reg, const uint8_t* data, uint8_t len) const;
    void readPayload(uint8_t* buffer, uint8_t len) const;
    void writePayload(const uint8_t* buffer, uint8_t len) const;
    void dumpPayload(const uint8_t* buffer, uint8_t len) const;

    void flushRx() const;
    void flushTx() const;
    void powerUp() const;
    void powerDown() const;
    void ceHigh() const;
    void ceLow() const;
    void setRxMode() const;
    void setTxMode() const;
    bool carrierDetected() const;
    void deselectOtherSpi() const;
    void stopConstCarrier() const;
    void startConstCarrier(uint8_t ch) const;
    void restoreDefaults();

    void drawToolChrome(const char* title, const char* hint) const;
    void drawBottomBar(const char* a, const char* b, const char* c, const char* d) const;
    int8_t pollBottomButton(uint16_t& x, uint16_t& y) const;
    bool waitTouchRelease() const;
};

extern Nrf24Interface nrf24_obj;

#endif
