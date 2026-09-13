#include "CC1101Interface.h"

#ifdef HAS_CC1101

#include <SPI.h>

#ifndef CC1101_CE_PIN
#define CC1101_CE_PIN 4
#endif
#ifndef CC1101_CSN_PIN
#define CC1101_CSN_PIN 5
#endif
#ifndef CC1101_GDO0_PIN
#define CC1101_GDO0_PIN 34
#endif

#define CC1101_IOCFG2   0x00
#define CC1101_IOCFG1   0x01
#define CC1101_IOCFG0   0x02
#define CC1101_FIFOTHR  0x03
#define CC1101_SYNC1    0x04
#define CC1101_SYNC0    0x05
#define CC1101_PKTLEN   0x06
#define CC1101_PKTCTRL1 0x07
#define CC1101_PKTCTRL0 0x08
#define CC1101_ADDR     0x09
#define CC1101_CHANNR   0x0A
#define CC1101_FSCTRL1  0x0B
#define CC1101_FSCTRL0  0x0C
#define CC1101_FREQ2    0x0D
#define CC1101_FREQ1    0x0E
#define CC1101_FREQ0    0x0F
#define CC1101_MDMCFG4  0x10
#define CC1101_MDMCFG3  0x11
#define CC1101_MDMCFG2  0x12
#define CC1101_MDMCFG1  0x13
#define CC1101_MDMCFG0  0x14
#define CC1101_DEVIATN  0x15
#define CC1101_MCSM2    0x16
#define CC1101_MCSM1    0x17
#define CC1101_MCSM0    0x18
#define CC1101_FOCCFG   0x19
#define CC1101_BSCFG    0x1A
#define CC1101_AGCTRL2  0x1B
#define CC1101_AGCTRL1  0x1C
#define CC1101_AGCTRL0  0x1D
#define CC1101_WOREVT1  0x1E
#define CC1101_WOREVT0  0x1F
#define CC1101_WORCTRL  0x20
#define CC1101_FREND1   0x21
#define CC1101_FREND0   0x22
#define CC1101_FSCAL3   0x23
#define CC1101_FSCAL2   0x24
#define CC1101_FSCAL1   0x25
#define CC1101_FSCAL0   0x26
#define CC1101_RCCTRL1  0x27
#define CC1101_RCCTRL0  0x28
#define CC1101_FSTEST   0x29
#define CC1101_PTEST    0x2A
#define CC1101_AGCTEST  0x2B
#define CC1101_TEST2    0x2C
#define CC1101_TEST1    0x2D
#define CC1101_TEST0    0x2E
#define CC1101_PARTNUM  0x30
#define CC1101_VERSION  0x31
#define CC1101_FREQEST  0x32
#define CC1101_LQI      0x33
#define CC1101_RSSI     0x34
#define CC1101_MARCSTATE 0x35
#define CC1101_WORTIME1 0x36
#define CC1101_WORTIME0 0x37
#define CC1101_PKTSTATUS 0x38
#define CC1101_VCO_VC_DAC 0x39
#define CC1101_TXBYTES  0x3A
#define CC1101_RXBYTES  0x3B
#define CC1101_RCCTRL1_STATUS 0x3C
#define CC1101_PATABLE 0x3E
#define CC1101_TXFIFO  0x3F
#define CC1101_RXFIFO  0x3F

#define CC1101_BURST_WRITE 0x40
#define CC1101_READ_SINGLE 0x80
#define CC1101_READ_BURST  0xC0

static SPIClass cc1101Spi = SPIClass(HSPI);

CC1101Interface::CC1101Interface() {
  pinMode(CC1101_CE_PIN, OUTPUT);
  pinMode(CC1101_CSN_PIN, OUTPUT);
  pinMode(CC1101_GDO0_PIN, INPUT);
  digitalWrite(CC1101_CE_PIN, LOW);
  digitalWrite(CC1101_CSN_PIN, HIGH);
}

bool CC1101Interface::begin() {
  cc1101Spi.begin();
  delay(10);
  reset();
  delay(10);

  uint8_t part = readRegister(CC1101_PARTNUM);
  uint8_t version = readRegister(CC1101_VERSION);
  uint8_t status = readRegister(CC1101_PKTSTATUS);
  initialized_ = (part == 0x00 || part == 0x01 || part == 0x14 || part == 0x12 || part == 0xCC) && (version != 0xFF) && (status != 0xFF);
  present_ = initialized_;

  if (!initialized_) {
    Serial.print("[CC1101] not detected part=");
    Serial.print(part, HEX);
    Serial.print(" version=");
    Serial.print(version, HEX);
    Serial.print(" pktstatus=");
    Serial.println(status, HEX);
    return false;
  }

  writeRegister(CC1101_MCSM2, 0x07);
  writeRegister(CC1101_MCSM1, 0x30);
  writeRegister(CC1101_MCSM0, 0x18);
  writeRegister(CC1101_FIFOTHR, 0x07);
  writeRegister(CC1101_PKTLEN, 0x3F);
  writeRegister(CC1101_PKTCTRL1, 0x04);
  writeRegister(CC1101_PKTCTRL0, 0x05);
  writeRegister(CC1101_CHANNR, channel_);
  writeRegister(CC1101_FSCTRL1, 0x08);
  writeRegister(CC1101_FSCTRL0, 0x00);

  Serial.print("[CC1101] initialized part=");
  Serial.print(part, HEX);
  Serial.print(" version=");
  Serial.println(version, HEX);
  return true;
}

bool CC1101Interface::isPresent() const {
  return present_;
}

void CC1101Interface::reset() {
  writeRegister(CC1101_MCSM2, 0x07);
  writeRegister(CC1101_MCSM1, 0x30);
  writeRegister(CC1101_MCSM0, 0x18);
  writeRegister(CC1101_FIFOTHR, 0x07);
  writeRegister(CC1101_CHANNR, 0x00);
  writeRegister(CC1101_PKTCTRL0, 0x05);
}

void CC1101Interface::setChannel(uint8_t channel) {
  channel_ = channel;
  if (initialized_) writeRegister(CC1101_CHANNR, channel_);
}

void CC1101Interface::setPowerLevel(uint8_t level) {
  powerLevel_ = level;
  if (!initialized_) return;

  uint8_t bits = level & 0x03;
  writeRegister(CC1101_PATABLE, bits == 0 ? 0x00 : (bits == 1 ? 0x47 : (bits == 2 ? 0x86 : 0xC3)));
}

uint8_t CC1101Interface::readStatus() const {
  return readRegister(CC1101_PKTSTATUS);
}

void CC1101Interface::runDiagnostic() {
  if (!initialized_) {
    Serial.println("[CC1101] diagnostic skipped: not present");
    return;
  }
  uint8_t status = readStatus();
  uint8_t lqi = readRegister(CC1101_LQI);
  uint8_t rssi = readRegister(CC1101_RSSI);
  Serial.print("[CC1101] status=");
  Serial.println(status, HEX);
  Serial.print("[CC1101] lqi=");
  Serial.print(lqi, HEX);
  Serial.print(" rssi=");
  Serial.println(rssi, HEX);
  Serial.print("[CC1101] part=");
  Serial.println(readRegister(CC1101_PARTNUM), HEX);
  Serial.print("[CC1101] version=");
  Serial.println(readRegister(CC1101_VERSION), HEX);
}

void CC1101Interface::runScan() {
  if (!initialized_) return;

  Serial.println("[CC1101] sweeping channels");
  for (uint8_t ch = 0; ch < 32; ++ch) {
    setChannel(ch);
    delay(10);
    uint8_t lqi = readRegister(CC1101_LQI);
    uint8_t status = readRegister(CC1101_PKTSTATUS);
    if (lqi != 0xFF || status != 0x00) {
      Serial.print("[CC1101] channel ");
      Serial.print(ch);
      Serial.print(" lqi=");
      Serial.print(lqi, HEX);
      Serial.print(" pkt=");
      Serial.println(status, HEX);
    }
  }
  setChannel(channel_);
}

void CC1101Interface::runRxTest() {
  if (!initialized_) return;
  Serial.println("[CC1101] RX test: listening on current channel");
  for (uint8_t i = 0; i < 20; ++i) {
    uint8_t status = readRegister(CC1101_PKTSTATUS);
    if (status & 0x01) {
      uint8_t len = readRegister(CC1101_RXBYTES);
      uint8_t payload[64] = {0};
      if (len > sizeof(payload)) len = sizeof(payload);
      readBurst(CC1101_RXFIFO, payload, len);
      Serial.print("[CC1101] RX bytes=");
      Serial.print(len);
      Serial.print(" data=");
      for (uint8_t j = 0; j < len; ++j) {
        if (payload[j] < 0x10) Serial.print('0');
        Serial.print(payload[j], HEX);
        Serial.print(' ');
      }
      Serial.println();
      break;
    }
    delay(50);
  }
}

void CC1101Interface::runJammerTest() {
  if (!initialized_) return;
  Serial.println("[CC1101] jammer test: emitting repetitive pattern");

  uint8_t pkt[16];
  for (uint8_t i = 0; i < sizeof(pkt); ++i) {
    pkt[i] = (uint8_t)(0xAA ^ i);
  }

  writeBurst(CC1101_TXFIFO, pkt, sizeof(pkt));
  digitalWrite(CC1101_CE_PIN, HIGH);
  delayMicroseconds(15);
  digitalWrite(CC1101_CE_PIN, LOW);
  Serial.println("[CC1101] TX burst sent");
}

uint8_t CC1101Interface::readRegister(uint8_t reg) const {
  digitalWrite(CC1101_CSN_PIN, LOW);
  cc1101Spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  cc1101Spi.transfer(CC1101_READ_SINGLE | (reg & 0x3F));
  uint8_t value = cc1101Spi.transfer(0xFF);
  cc1101Spi.endTransaction();
  digitalWrite(CC1101_CSN_PIN, HIGH);
  return value;
}

void CC1101Interface::writeRegister(uint8_t reg, uint8_t value) const {
  digitalWrite(CC1101_CSN_PIN, LOW);
  cc1101Spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  cc1101Spi.transfer(reg & 0x3F);
  cc1101Spi.transfer(value);
  cc1101Spi.endTransaction();
  digitalWrite(CC1101_CSN_PIN, HIGH);
}

void CC1101Interface::readBurst(uint8_t reg, uint8_t* buffer, uint8_t len) const {
  if (buffer == nullptr || len == 0) return;

  digitalWrite(CC1101_CSN_PIN, LOW);
  cc1101Spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  cc1101Spi.transfer(CC1101_READ_BURST | (reg & 0x3F));
  for (uint8_t i = 0; i < len; ++i) {
    buffer[i] = cc1101Spi.transfer(0xFF);
  }
  cc1101Spi.endTransaction();
  digitalWrite(CC1101_CSN_PIN, HIGH);
}

void CC1101Interface::writeBurst(uint8_t reg, const uint8_t* buffer, uint8_t len) const {
  if (buffer == nullptr || len == 0) return;

  digitalWrite(CC1101_CSN_PIN, LOW);
  cc1101Spi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  cc1101Spi.transfer(CC1101_BURST_WRITE | (reg & 0x3F));
  for (uint8_t i = 0; i < len; ++i) {
    cc1101Spi.transfer(buffer[i]);
  }
  cc1101Spi.endTransaction();
  digitalWrite(CC1101_CSN_PIN, HIGH);
}

CC1101Interface cc1101_obj;

#endif
