#pragma once

#include <Arduino.h>
#include "configs.h"

#ifdef HAS_CC1101

class CC1101Interface {
  public:
    CC1101Interface();

    bool begin();
    bool isPresent() const;
    void reset();
    void setChannel(uint8_t channel);
    void setPowerLevel(uint8_t level);
    uint8_t readStatus() const;
    void runDiagnostic();
    void runScan();
    void runRxTest();
    void runJammerTest();

  private:
    bool initialized_ = false;
    bool present_ = false;
    uint8_t channel_ = 0;
    uint8_t powerLevel_ = 0;
    uint8_t readRegister(uint8_t reg) const;
    void writeRegister(uint8_t reg, uint8_t value) const;
    void readBurst(uint8_t reg, uint8_t* buffer, uint8_t len) const;
    void writeBurst(uint8_t reg, const uint8_t* buffer, uint8_t len) const;
};

extern CC1101Interface cc1101_obj;

#endif
