#include "Pn532Interface.h"

#ifdef HAS_PN532

#include <SPI.h>

#ifndef PN532_CE_PIN
#define PN532_CE_PIN 4
#endif
#ifndef PN532_CSN_PIN
#define PN532_CSN_PIN 5
#endif
#ifndef PN532_IRQ_PIN
#define PN532_IRQ_PIN 34
#endif

#define PN532_COMMAND_GETFIRMWAREVERSION 0x02
#define PN532_COMMAND_SAMCONFIGURATION 0x14
#define PN532_COMMAND_INLISTPASSIVETARGET 0x4A
#define PN532_SPI_STATREAD 0x02
#define PN532_SPI_DATAWRITE 0x01
#define PN532_SPI_DATAREAD 0x03
#define PN532_SPI_READY 0x01

static SPIClass pn532Spi = SPIClass(HSPI);

Pn532Interface::Pn532Interface() {
  pinMode(PN532_CE_PIN, OUTPUT);
  pinMode(PN532_CSN_PIN, OUTPUT);
  pinMode(PN532_IRQ_PIN, INPUT);
  digitalWrite(PN532_CE_PIN, LOW);
  digitalWrite(PN532_CSN_PIN, HIGH);
}

bool Pn532Interface::begin() {
  pn532Spi.begin();
  delay(10);
  reset();
  delay(10);

  uint8_t version = readVersion();
  initialized_ = version != 0xFF;
  present_ = initialized_;

  if (!initialized_) {
    Serial.println("[PN532] not detected");
    return false;
  }

  uint8_t config[] = {0x00, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x00};
  uint8_t response[16] = {0};
  sendCommand(config, sizeof(config), response, sizeof(response));

  Serial.println("[PN532] initialized");
  return true;
}

bool Pn532Interface::isPresent() const {
  return present_;
}

void Pn532Interface::reset() {
  digitalWrite(PN532_CSN_PIN, LOW);
  pn532Spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  pn532Spi.transfer(0x00);
  pn532Spi.endTransaction();
  digitalWrite(PN532_CSN_PIN, HIGH);
}

uint8_t Pn532Interface::readVersion() const {
  uint8_t cmd[] = {PN532_COMMAND_GETFIRMWAREVERSION};
  uint8_t response[16] = {0};
  uint8_t len = sendCommand(cmd, sizeof(cmd), response, sizeof(response));
  if (len >= 4) {
    return response[2];
  }
  return 0xFF;
}

void Pn532Interface::runDiagnostic() {
  if (!initialized_) {
    Serial.println("[PN532] diagnostic skipped: not present");
    return;
  }
  uint8_t version = readVersion();
  Serial.print("[PN532] version=");
  Serial.println(version, HEX);
  Serial.print("[PN532] state=");
  Serial.println(present_ ? "ready" : "offline");
}

void Pn532Interface::runScan() {
  if (!initialized_) return;

  uint8_t cmd[] = {0xD4, PN532_COMMAND_INLISTPASSIVETARGET, 0x01, 0x00};
  uint8_t response[32] = {0};
  uint8_t len = sendCommand(cmd, sizeof(cmd), response, sizeof(response));

  if (len >= 2 && response[0] == 0x01) {
    Serial.print("[PN532] tag detected len=");
    Serial.println(len);
    for (uint8_t i = 0; i < len; ++i) {
      if (response[i] < 0x10) Serial.print('0');
      Serial.print(response[i], HEX);
      Serial.print(' ');
    }
    Serial.println();
  } else {
    Serial.println("[PN532] no tag in range");
  }
}

bool Pn532Interface::readCardId(uint8_t id[7], uint8_t& len) {
  if (!initialized_ || id == nullptr) return false;

  uint8_t cmd[] = {0xD4, PN532_COMMAND_INLISTPASSIVETARGET, 0x01, 0x00};
  uint8_t response[32] = {0};
  uint8_t responseLen = sendCommand(cmd, sizeof(cmd), response, sizeof(response));

  if (responseLen < 2 || response[0] != 0x01) {
    len = 0;
    return false;
  }

  len = response[1];
  if (len > 7) len = 7;
  for (uint8_t i = 0; i < len; ++i) {
    id[i] = response[2 + i];
  }
  return true;
}

uint8_t Pn532Interface::readRegister(uint8_t reg) const {
  (void)reg;
  return 0;
}

void Pn532Interface::writeRegister(uint8_t reg, uint8_t value) const {
  (void)reg;
  (void)value;
}

uint8_t Pn532Interface::sendCommand(const uint8_t* cmd, uint8_t len, uint8_t* response, uint8_t maxLen) const {
  // The fixed local frame is 64 bytes, including the PN532 header and TFI.
  if (cmd == nullptr || response == nullptr || maxLen == 0 || len > 57) return 0;

  uint8_t frame[64] = {0};
  frame[0] = 0x00;
  frame[1] = 0x00;
  frame[2] = 0xFF;
  frame[3] = 0x00;
  frame[4] = len + 1;
  frame[5] = 0xD4;
  memcpy(&frame[6], cmd, len);

  digitalWrite(PN532_CSN_PIN, LOW);
  pn532Spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  for (uint8_t i = 0; i < frame[4] + 6; ++i) {
    pn532Spi.transfer(frame[i]);
  }
  pn532Spi.endTransaction();
  digitalWrite(PN532_CSN_PIN, HIGH);

  delay(20);

  digitalWrite(PN532_CSN_PIN, LOW);
  pn532Spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  uint8_t header[6] = {0};
  for (uint8_t i = 0; i < 6; ++i) {
    header[i] = pn532Spi.transfer(0xFF);
  }
  // Reject a misaligned SPI response before interpreting its length byte.
  if (header[0] != 0x00 || header[1] != 0x00 || header[2] != 0xFF) {
    pn532Spi.endTransaction();
    digitalWrite(PN532_CSN_PIN, HIGH);
    return 0;
  }
  uint8_t count = header[4];
  if (count > maxLen) {
    count = maxLen;
  }
  for (uint8_t i = 0; i < count; ++i) {
    response[i] = pn532Spi.transfer(0xFF);
  }
  pn532Spi.endTransaction();
  digitalWrite(PN532_CSN_PIN, HIGH);

  return count;
}

Pn532Interface pn532_obj;

#endif
