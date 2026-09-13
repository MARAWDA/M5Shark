#pragma once

#include <Arduino.h>
#include "configs.h"

#ifdef HAS_PN532

class Pn532Interface {
  public:
    Pn532Interface();

    bool begin();
    bool isPresent() const;
    void reset();
    uint8_t readVersion() const;
    void runDiagnostic();
    void runScan();
    bool readCardId(uint8_t id[7], uint8_t& len);

  private:
    bool initialized_ = false;
    bool present_ = false;
    uint8_t readRegister(uint8_t reg) const;
    void writeRegister(uint8_t reg, uint8_t value) const;
    uint8_t sendCommand(const uint8_t* cmd, uint8_t len, uint8_t* response, uint8_t maxLen) const;
};

extern Pn532Interface pn532_obj;

#endif
