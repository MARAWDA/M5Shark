#pragma once

#include <Arduino.h>
#include "configs.h"

#ifdef HAS_NRF24

class Nrf24Interface {
  public:
    Nrf24Interface();

    bool begin();
    bool isPresent() const;
    void reset();
    void setChannel(uint8_t channel);
    void setPowerLevel(uint8_t level);
    uint8_t readStatus() const;
    void runDiagnostic();
    void runChannelScan();
    void runCarrierHeatmap();
    void runRxTest();
    void runJammerTest();

  private:
    bool initialized_ = false;
    bool present_ = false;
    uint8_t channel_ = 76;
    uint8_t powerLevel_ = 3;
    uint8_t readRegister(uint8_t reg) const;
    void writeRegister(uint8_t reg, uint8_t value) const;
    void readPayload(uint8_t* buffer, uint8_t len) const;
    void writePayload(const uint8_t* buffer, uint8_t len) const;
    void dumpPayload(const uint8_t* buffer, uint8_t len) const;
};

extern Nrf24Interface nrf24_obj;

#endif
