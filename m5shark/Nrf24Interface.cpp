#include "Nrf24Interface.h"

#ifdef HAS_NRF24

#include <SPI.h>

#ifndef NRF24_CE_PIN
#define NRF24_CE_PIN 4
#endif
#ifndef NRF24_CSN_PIN
#define NRF24_CSN_PIN 5
#endif
#ifndef NRF24_IRQ_PIN
#define NRF24_IRQ_PIN 34
#endif

#define NRF24_CONFIG      0x00
#define NRF24_EN_AA       0x01
#define NRF24_EN_RXADDR   0x02
#define NRF24_SETUP_AW    0x03
#define NRF24_SETUP_RETR  0x04
#define NRF24_RF_CH       0x05
#define NRF24_RF_SETUP    0x06
#define NRF24_STATUS      0x07
#define NRF24_OBSERVE_TX  0x08
#define NRF24_CD          0x09
#define NRF24_RX_ADDR_P0  0x0A
#define NRF24_RX_ADDR_P1  0x0B
#define NRF24_RX_ADDR_P2  0x0C
#define NRF24_RX_ADDR_P3  0x0D
#define NRF24_RX_ADDR_P4  0x0E
#define NRF24_RX_ADDR_P5  0x0F
#define NRF24_TX_ADDR     0x10
#define NRF24_RX_PW_P0    0x11
#define NRF24_RX_PW_P1    0x12
#define NRF24_RX_PW_P2    0x13
#define NRF24_RX_PW_P3    0x14
#define NRF24_RX_PW_P4    0x15
#define NRF24_RX_PW_P5    0x16
#define NRF24_FIFO_STATUS 0x17
#define NRF24_DYNPD       0x1C
#define NRF24_FEATURE     0x1D

#define NRF24_CMD_R_REGISTER     0x00
#define NRF24_CMD_W_REGISTER     0x20
#define NRF24_CMD_R_RX_PAYLOAD   0x61
#define NRF24_CMD_W_TX_PAYLOAD   0xA0
#define NRF24_CMD_FLUSH_TX       0xE1
#define NRF24_CMD_FLUSH_RX       0xE2
#define NRF24_CMD_REUSE_TX_PL    0xE3
#define NRF24_CMD_R_RX_PL_WID    0x60
#define NRF24_CMD_W_ACK_PAYLOAD  0xA8
#define NRF24_CMD_W_TX_PAYLOAD_NO_ACK 0xB0
#define NRF24_CMD_NOP            0xFF

static SPIClass nrfSpi = SPIClass(HSPI);

Nrf24Interface::Nrf24Interface() {
  pinMode(NRF24_CE_PIN, OUTPUT);
  pinMode(NRF24_CSN_PIN, OUTPUT);
  pinMode(NRF24_IRQ_PIN, INPUT);
  digitalWrite(NRF24_CE_PIN, LOW);
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

bool Nrf24Interface::begin() {
  nrfSpi.begin();
  delay(10);

  reset();
  delay(10);

  uint8_t config = readRegister(NRF24_CONFIG);
  uint8_t status = readRegister(NRF24_STATUS);

  initialized_ = (config != 0xFF) && (status != 0xFF);
  present_ = initialized_;

  if (!initialized_) {
    Serial.println("[NRF24] not detected");
    return false;
  }

  writeRegister(NRF24_CONFIG, 0x0F);
  writeRegister(NRF24_EN_AA, 0x00);
  writeRegister(NRF24_EN_RXADDR, 0x01);
  writeRegister(NRF24_SETUP_AW, 0x03);
  writeRegister(NRF24_SETUP_RETR, 0x5F);
  writeRegister(NRF24_RF_CH, channel_);
  writeRegister(NRF24_RF_SETUP, 0x26);
  writeRegister(NRF24_STATUS, 0x70);

  Serial.println("[NRF24] initialized");
  return true;
}

bool Nrf24Interface::isPresent() const {
  return present_;
}

void Nrf24Interface::reset() {
  writeRegister(NRF24_CONFIG, 0x08);
  writeRegister(NRF24_EN_AA, 0x00);
  writeRegister(NRF24_EN_RXADDR, 0x00);
  writeRegister(NRF24_SETUP_AW, 0x03);
  writeRegister(NRF24_SETUP_RETR, 0x00);
  writeRegister(NRF24_RF_CH, 0x00);
  writeRegister(NRF24_RF_SETUP, 0x00);
  writeRegister(NRF24_STATUS, 0x70);
  writeRegister(NRF24_FIFO_STATUS, 0x11);
  writeRegister(NRF24_DYNPD, 0x00);
  writeRegister(NRF24_FEATURE, 0x00);
}

void Nrf24Interface::setChannel(uint8_t channel) {
  channel_ = channel;
  if (initialized_) writeRegister(NRF24_RF_CH, channel_);
}

void Nrf24Interface::setPowerLevel(uint8_t level) {
  powerLevel_ = level;
  if (initialized_) {
    uint8_t v = readRegister(NRF24_RF_SETUP) & 0xF1;
    v |= ((level & 0x03) << 1);
    writeRegister(NRF24_RF_SETUP, v);
  }
}

uint8_t Nrf24Interface::readStatus() const {
  return readRegister(NRF24_STATUS);
}

void Nrf24Interface::runDiagnostic() {
  if (!initialized_) {
    Serial.println("[NRF24] diagnostic skipped: not present");
    return;
  }

  Serial.print("[NRF24] status=");
  Serial.println(readStatus(), HEX);
  Serial.print("[NRF24] config=");
  Serial.println(readRegister(NRF24_CONFIG), HEX);
  Serial.print("[NRF24] rf_setup=");
  Serial.println(readRegister(NRF24_RF_SETUP), HEX);
}

void Nrf24Interface::runChannelScan() {
  if (!initialized_) return;

  Serial.println("[NRF24] scanning 125 channels");
  for (uint8_t ch = 0; ch < 125; ++ch) {
    setChannel(ch);
    delayMicroseconds(150);
    uint8_t cd = readRegister(NRF24_CD) & 0x01;
    if (cd) {
      Serial.print("[NRF24] carrier on channel ");
      Serial.println(ch);
    }
  }

  setChannel(channel_);
  Serial.println("[NRF24] channel scan complete");
}

void Nrf24Interface::runCarrierHeatmap() {
  if (!initialized_) {
    Serial.println("[NRF24] carrier heatmap skipped: not present");
    return;
  }

  // The nRF24 has 125 selectable channels. Five channels per bucket keeps the
  // serial report compact while preserving the shape of the spectrum.
  uint8_t buckets[25] = {};
  uint16_t active_channels = 0;
  Serial.println("[NRF24] carrier heatmap: 25 buckets, 5 channels each");
  for (uint8_t ch = 0; ch < 125; ++ch) {
    setChannel(ch);
    // Carrier Detect is sampled after a short settle time; this mode never TXes.
    delayMicroseconds(150);
    if (readRegister(NRF24_CD) & 0x01) {
      ++buckets[ch / 5];
      ++active_channels;
    }
  }
  setChannel(channel_);
  for (uint8_t bucket = 0; bucket < 25; ++bucket) {
    const uint8_t first = bucket * 5;
    const uint8_t last = first + 4;
    Serial.print("[NRF24] CH ");
    Serial.print(first);
    Serial.print('-');
    Serial.print(last);
    Serial.print(" ");
    for (uint8_t i = 0; i < buckets[bucket]; ++i) Serial.print('#');
    Serial.print(" (");
    Serial.print(buckets[bucket]);
    Serial.println(")");
  }
  Serial.print("[NRF24] active channels=");
  Serial.println(active_channels);
}

void Nrf24Interface::runRxTest() {
  if (!initialized_) return;

  Serial.println("[NRF24] RX test: listening for payloads");
  uint8_t config = readRegister(NRF24_CONFIG);
  writeRegister(NRF24_CONFIG, (config | 0x02) & 0x7F);
  writeRegister(NRF24_STATUS, 0x70);

  for (uint8_t i = 0; i < 40; ++i) {
    uint8_t status = readRegister(NRF24_STATUS);
    if (status & 0x40) {
      uint8_t payloadWidth = 0;
      digitalWrite(NRF24_CSN_PIN, LOW);
      nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
      nrfSpi.transfer(NRF24_CMD_R_RX_PL_WID);
      payloadWidth = nrfSpi.transfer(0xFF);
      nrfSpi.endTransaction();
      digitalWrite(NRF24_CSN_PIN, HIGH);

      payloadWidth = min((uint8_t)32, payloadWidth);
      uint8_t payload[32] = {0};
      readPayload(payload, payloadWidth);

      Serial.print("[NRF24] RX payload len=");
      Serial.print(payloadWidth);
      Serial.print(" data=");
      dumpPayload(payload, payloadWidth);
      Serial.println();
      writeRegister(NRF24_STATUS, 0x70);
      break;
    }
    delay(50);
  }

  writeRegister(NRF24_CONFIG, config);
  Serial.println("[NRF24] RX test complete");
}

void Nrf24Interface::runJammerTest() {
  if (!initialized_) return;

  Serial.println("[NRF24] jammer test: transmitting continuous noise");
  uint8_t tx[32];
  for (uint8_t i = 0; i < sizeof(tx); ++i) {
    tx[i] = (uint8_t)(0xFF - i);
  }

  uint8_t config = readRegister(NRF24_CONFIG);
  writeRegister(NRF24_CONFIG, (config & 0xFD) | 0x01);
  writeRegister(NRF24_STATUS, 0x70);

  for (uint8_t i = 0; i < 10; ++i) {
    writePayload(tx, sizeof(tx));
    digitalWrite(NRF24_CE_PIN, HIGH);
    delayMicroseconds(15);
    digitalWrite(NRF24_CE_PIN, LOW);
    delay(25);
    uint8_t status = readRegister(NRF24_STATUS);
    Serial.print("[NRF24] tx status=");
    Serial.println(status, HEX);
  }

  writeRegister(NRF24_CONFIG, config);
  Serial.println("[NRF24] jammer test complete");
}

uint8_t Nrf24Interface::readRegister(uint8_t reg) const {
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_R_REGISTER | (reg & 0x1F));
  uint8_t value = nrfSpi.transfer(0xFF);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
  return value;
}

void Nrf24Interface::writeRegister(uint8_t reg, uint8_t value) const {
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_W_REGISTER | (reg & 0x1F));
  nrfSpi.transfer(value);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::readPayload(uint8_t* buffer, uint8_t len) const {
  if (buffer == nullptr || len == 0) return;

  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_R_RX_PAYLOAD);
  for (uint8_t i = 0; i < len; ++i) {
    buffer[i] = nrfSpi.transfer(0xFF);
  }
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::writePayload(const uint8_t* buffer, uint8_t len) const {
  if (buffer == nullptr || len == 0) return;

  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_W_TX_PAYLOAD);
  for (uint8_t i = 0; i < len; ++i) {
    nrfSpi.transfer(buffer[i]);
  }
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::dumpPayload(const uint8_t* buffer, uint8_t len) const {
  if (buffer == nullptr || len == 0) return;

  for (uint8_t i = 0; i < len; ++i) {
    if (buffer[i] < 0x10) Serial.print('0');
    Serial.print(buffer[i], HEX);
    Serial.print(' ');
  }
}

Nrf24Interface nrf24_obj;

#endif
