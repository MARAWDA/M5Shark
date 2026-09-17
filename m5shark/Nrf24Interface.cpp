#include "Nrf24Interface.h"

#ifdef HAS_NRF24

#include <SPI.h>
#include "Display.h"
#include "SharkTheme.h"

extern Display display_obj;

#ifndef NRF24_CE_PIN
#define NRF24_CE_PIN 4
#endif
#ifndef NRF24_CSN_PIN
#define NRF24_CSN_PIN 5
#endif
#ifndef NRF24_IRQ_PIN
#define NRF24_IRQ_PIN 34
#endif
#ifndef NRF24_SCK_PIN
#define NRF24_SCK_PIN -1
#endif
#ifndef NRF24_MISO_PIN
#define NRF24_MISO_PIN -1
#endif
#ifndef NRF24_MOSI_PIN
#define NRF24_MOSI_PIN -1
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
#define NRF24_RPD         0x09
#define NRF24_CD          0x09
#define NRF24_RX_ADDR_P0  0x0A
#define NRF24_RX_ADDR_P1  0x0B
#define NRF24_TX_ADDR     0x10
#define NRF24_RX_PW_P0    0x11
#define NRF24_RX_PW_P1    0x12
#define NRF24_FIFO_STATUS 0x17
#define NRF24_DYNPD       0x1C
#define NRF24_FEATURE     0x1D

#define NRF24_CMD_R_REGISTER    0x00
#define NRF24_CMD_W_REGISTER    0x20
#define NRF24_CMD_R_RX_PAYLOAD  0x61
#define NRF24_CMD_W_TX_PAYLOAD  0xA0
#define NRF24_CMD_FLUSH_TX      0xE1
#define NRF24_CMD_FLUSH_RX      0xE2
#define NRF24_CMD_REUSE_TX_PL   0xE3
#define NRF24_CMD_R_RX_PL_WID   0x60
#define NRF24_CMD_NOP           0xFF

static SPIClass nrfSpi = SPIClass(HSPI);

static constexpr int16_t NRF_BTN_Y = 260;
static constexpr int16_t NRF_BTN_H = 56;
static constexpr int16_t NRF_CONTENT_TOP = 36;

Nrf24Interface::Nrf24Interface() {
  pinMode(NRF24_CE_PIN, OUTPUT);
  pinMode(NRF24_CSN_PIN, OUTPUT);
  pinMode(NRF24_IRQ_PIN, INPUT);
  digitalWrite(NRF24_CE_PIN, LOW);
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::deselectOtherSpi() const {
#ifdef SD_CS
  if (SD_CS >= 0) {
    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);
  }
#endif
#ifdef CC1101_CS
  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, HIGH);
#endif
#ifdef PN532_CS
  pinMode(PN532_CS, OUTPUT);
  digitalWrite(PN532_CS, HIGH);
#endif
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

bool Nrf24Interface::begin() {
  deselectOtherSpi();
#if NRF24_SCK_PIN >= 0
  nrfSpi.begin(NRF24_SCK_PIN, NRF24_MISO_PIN, NRF24_MOSI_PIN, -1);
#else
  nrfSpi.begin();
#endif
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

bool Nrf24Interface::isPresent() const { return present_; }

void Nrf24Interface::reset() {
  writeRegister(NRF24_CONFIG, 0x08);
  writeRegister(NRF24_EN_AA, 0x00);
  writeRegister(NRF24_EN_RXADDR, 0x00);
  writeRegister(NRF24_SETUP_AW, 0x03);
  writeRegister(NRF24_SETUP_RETR, 0x00);
  writeRegister(NRF24_RF_CH, 0x00);
  writeRegister(NRF24_RF_SETUP, 0x00);
  writeRegister(NRF24_STATUS, 0x70);
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

uint8_t Nrf24Interface::readStatus() const { return readRegister(NRF24_STATUS); }

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

void Nrf24Interface::writeRegisterMulti(uint8_t reg, const uint8_t* data, uint8_t len) const {
  if (!data || !len) return;
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_W_REGISTER | (reg & 0x1F));
  for (uint8_t i = 0; i < len; ++i) nrfSpi.transfer(data[i]);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::readPayload(uint8_t* buffer, uint8_t len) const {
  if (!buffer || !len) return;
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_R_RX_PAYLOAD);
  for (uint8_t i = 0; i < len; ++i) buffer[i] = nrfSpi.transfer(0xFF);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::writePayload(const uint8_t* buffer, uint8_t len) const {
  if (!buffer || !len) return;
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_W_TX_PAYLOAD);
  for (uint8_t i = 0; i < len; ++i) nrfSpi.transfer(buffer[i]);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::dumpPayload(const uint8_t* buffer, uint8_t len) const {
  if (!buffer || !len) return;
  for (uint8_t i = 0; i < len; ++i) {
    if (buffer[i] < 0x10) Serial.print('0');
    Serial.print(buffer[i], HEX);
    Serial.print(' ');
  }
}

void Nrf24Interface::flushRx() const {
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_FLUSH_RX);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::flushTx() const {
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_FLUSH_TX);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
}

void Nrf24Interface::powerUp() const {
  writeRegister(NRF24_CONFIG, readRegister(NRF24_CONFIG) | 0x02);
  delayMicroseconds(1500);
}

void Nrf24Interface::powerDown() const {
  writeRegister(NRF24_CONFIG, readRegister(NRF24_CONFIG) & ~0x02);
}

void Nrf24Interface::ceHigh() const { digitalWrite(NRF24_CE_PIN, HIGH); }
void Nrf24Interface::ceLow() const { digitalWrite(NRF24_CE_PIN, LOW); }

void Nrf24Interface::setRxMode() const {
  writeRegister(NRF24_CONFIG, (readRegister(NRF24_CONFIG) | 0x03));
  ceHigh();
  delayMicroseconds(130);
}

void Nrf24Interface::setTxMode() const {
  ceLow();
  writeRegister(NRF24_CONFIG, (readRegister(NRF24_CONFIG) | 0x02) & ~0x01);
  delayMicroseconds(150);
}

bool Nrf24Interface::carrierDetected() const {
  return (readRegister(NRF24_RPD) & 0x01) != 0;
}

void Nrf24Interface::startConstCarrier(uint8_t ch) const {
  // CONT_WAVE + PLL_LOCK via RF_SETUP bits; max power 2Mbps.
  ceLow();
  writeRegister(NRF24_EN_AA, 0x00);
  writeRegister(NRF24_SETUP_RETR, 0x00);
  writeRegister(NRF24_RF_CH, ch);
  writeRegister(NRF24_RF_SETUP, 0x8E); // CONT_WAVE|PLL_LOCK|2Mbps|0dBm
  writeRegister(NRF24_CONFIG, 0x02);   // PWR_UP, PTX
  delayMicroseconds(1500);
  // Seed FIFO then REUSE_TX_PL so carrier stays up while CE high.
  uint8_t noise[2] = {0xFF, 0x00};
  writePayload(noise, 2);
  digitalWrite(NRF24_CSN_PIN, LOW);
  nrfSpi.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  nrfSpi.transfer(NRF24_CMD_REUSE_TX_PL);
  nrfSpi.endTransaction();
  digitalWrite(NRF24_CSN_PIN, HIGH);
  ceHigh();
}

void Nrf24Interface::stopConstCarrier() const {
  ceLow();
  writeRegister(NRF24_RF_SETUP, 0x0E); // clear CONT_WAVE/PLL_LOCK, keep 2Mbps max
  flushTx();
  writeRegister(NRF24_STATUS, 0x70);
}

void Nrf24Interface::restoreDefaults() {
  ceLow();
  stopConstCarrier();
  writeRegister(NRF24_SETUP_AW, 0x03);
  writeRegister(NRF24_EN_AA, 0x00);
  writeRegister(NRF24_EN_RXADDR, 0x01);
  writeRegister(NRF24_SETUP_RETR, 0x5F);
  writeRegister(NRF24_RF_CH, channel_);
  writeRegister(NRF24_RF_SETUP, 0x26);
  writeRegister(NRF24_CONFIG, 0x0E); // powered, PRX off CRC 2-byte
  writeRegister(NRF24_STATUS, 0x70);
  flushRx();
  flushTx();
}

void Nrf24Interface::drawToolChrome(const char* title, const char* hint) const {
  TFT_eSPI& tft = display_obj.tft;
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, tft.width(), 28, WD_PANEL);
  tft.setFreeFont(NULL);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(WD_CYAN, WD_PANEL);
  tft.drawString(title, 6, 8, 2);
  if (hint && *hint) {
    tft.setTextColor(WD_DIM, WD_PANEL);
    tft.drawString(hint, 6, 30, 1);
  }
}

void Nrf24Interface::drawBottomBar(const char* a, const char* b, const char* c, const char* d) const {
  TFT_eSPI& tft = display_obj.tft;
  const int16_t w = tft.width();
  const int16_t bw = w / 4;
  const char* labels[4] = {a, b, c, d};
  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t x = i * bw;
    const uint16_t fill = (i == 3) ? WD_CYAN : WD_SURFACE;
    tft.fillRoundRect(x + 2, NRF_BTN_Y, bw - 4, NRF_BTN_H, 4, fill);
    tft.drawRoundRect(x + 2, NRF_BTN_Y, bw - 4, NRF_BTN_H, 4, i == 3 ? WD_CYAN : WD_EDGE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(i == 3 ? TFT_BLACK : WD_BONE, fill);
    tft.drawString(labels[i] ? labels[i] : "", x + bw / 2, NRF_BTN_Y + NRF_BTN_H / 2, 1);
  }
  tft.setTextDatum(TL_DATUM);
}

int8_t Nrf24Interface::pollBottomButton(uint16_t& x, uint16_t& y) const {
  if (!display_obj.updateTouch(&x, &y)) return -1;
  if (y < (uint16_t)NRF_BTN_Y) return -1;
  const int16_t w = display_obj.tft.width();
  const int16_t bw = w / 4;
  if (bw <= 0) return -1;
  int8_t idx = (int8_t)(x / bw);
  if (idx < 0) idx = 0;
  if (idx > 3) idx = 3;
  return idx;
}

bool Nrf24Interface::waitTouchRelease() const {
  uint16_t x, y;
  uint32_t start = millis();
  while (display_obj.updateTouch(&x, &y) && (millis() - start < 800)) {
    delay(10);
  }
  delay(40);
  return true;
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
    if (carrierDetected()) {
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
  uint8_t buckets[25] = {};
  uint16_t active_channels = 0;
  Serial.println("[NRF24] carrier heatmap: 25 buckets, 5 channels each");
  for (uint8_t ch = 0; ch < 125; ++ch) {
    setChannel(ch);
    delayMicroseconds(150);
    if (carrierDetected()) {
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
  for (uint8_t i = 0; i < sizeof(tx); ++i) tx[i] = (uint8_t)(0xFF - i);
  uint8_t config = readRegister(NRF24_CONFIG);
  writeRegister(NRF24_CONFIG, (config & 0xFD) | 0x01);
  writeRegister(NRF24_STATUS, 0x70);
  for (uint8_t i = 0; i < 10; ++i) {
    writePayload(tx, sizeof(tx));
    ceHigh();
    delayMicroseconds(15);
    ceLow();
    delay(25);
    Serial.print("[NRF24] tx status=");
    Serial.println(readRegister(NRF24_STATUS), HEX);
  }
  writeRegister(NRF24_CONFIG, config);
  Serial.println("[NRF24] jammer test complete");
}

// ── Evaware-style full-screen tools ───────────────────────────────────────

void Nrf24Interface::runSpectrumScanner() {
  TFT_eSPI& tft = display_obj.tft;
  if (!initialized_ && !begin()) {
    drawToolChrome("NRF SCANNER", "nRF24 not detected");
    drawBottomBar("-", "-", "-", "BACK");
    while (true) {
      uint16_t x, y;
      if (pollBottomButton(x, y) == 3) { waitTouchRelease(); break; }
      delay(20);
    }
    return;
  }

  constexpr int SCAN_CH = 85;
  uint8_t levels[SCAN_CH] = {};
  bool running = true;
  drawToolChrome("NRF SCANNER", "RPD 2.4GHz ch0-84  |  WiFi 1/6/11 marks");
  drawBottomBar("PAUSE", "CLR", "SWEEP", "BACK");

  const int16_t graphX = 4;
  const int16_t graphY = NRF_CONTENT_TOP + 14;
  const int16_t graphW = tft.width() - 8;
  const int16_t graphH = 200;
  const float barW = (float)graphW / (float)SCAN_CH;

  while (true) {
    if (running) {
      deselectOtherSpi();
      writeRegister(NRF24_EN_AA, 0x00);
      writeRegister(NRF24_RF_SETUP, 0x0F);
      for (int i = 0; i < SCAN_CH; ++i) {
        writeRegister(NRF24_RF_CH, (uint8_t)i);
        setRxMode();
        delayMicroseconds(80);
        ceLow();
        const uint8_t rpd = carrierDetected() ? 120 : 0;
        levels[i] = (uint8_t)((levels[i] + rpd) / 2);
      }
    }

    tft.fillRect(graphX, graphY, graphW, graphH, TFT_BLACK);
    tft.drawRect(graphX, graphY, graphW, graphH, WD_EDGE);
    // WiFi channel guides (NRF ch 12/37/62)
    for (int mark : {12, 37, 62}) {
      int16_t mx = graphX + (int16_t)(mark * barW);
      tft.drawFastVLine(mx, graphY, graphH, WD_DIM);
    }
    for (int i = 0; i < SCAN_CH; ++i) {
      int16_t h = (int16_t)((levels[i] * (graphH - 4)) / 120);
      if (h < 1 && levels[i] > 0) h = 1;
      int16_t x = graphX + (int16_t)(i * barW);
      int16_t y = graphY + graphH - 2 - h;
      uint16_t col = levels[i] > 80 ? WD_RED : (levels[i] > 30 ? WD_AMBER : WD_CYAN);
      tft.fillRect(x, y, max((int16_t)1, (int16_t)(barW - 1)), h, col);
    }
    tft.setTextColor(WD_BONE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(running ? "SCAN" : "PAUSED", graphX + 2, graphY - 12, 1);

    uint32_t until = millis() + 180;
    while (millis() < until) {
      uint16_t x, y;
      int8_t btn = pollBottomButton(x, y);
      if (btn < 0) { delay(10); continue; }
      waitTouchRelease();
      if (btn == 0) {
        running = !running;
        drawBottomBar(running ? "PAUSE" : "RUN", "CLR", "SWEEP", "BACK");
      } else if (btn == 1) {
        memset(levels, 0, sizeof(levels));
      } else if (btn == 2) {
        // one full serial dump
        runChannelScan();
      } else if (btn == 3) {
        restoreDefaults();
        return;
      }
      break;
    }
  }
}

void Nrf24Interface::runSpectrumAnalyzer() {
  TFT_eSPI& tft = display_obj.tft;
  if (!initialized_ && !begin()) {
    drawToolChrome("NRF ANALYZER", "nRF24 not detected");
    drawBottomBar("-", "-", "-", "BACK");
    while (true) {
      uint16_t x, y;
      if (pollBottomButton(x, y) == 3) { waitTouchRelease(); break; }
      delay(20);
    }
    return;
  }

  constexpr int BINS = 64;
  uint8_t hist[BINS] = {};
  uint8_t ch = 0;
  bool running = true;
  drawToolChrome("NRF ANALYZER", "Hopping spectrum 0-125");
  drawBottomBar("PAUSE", "CLR", "LOCK", "BACK");
  bool locked = false;

  const int16_t graphX = 4;
  const int16_t graphY = NRF_CONTENT_TOP + 14;
  const int16_t graphW = tft.width() - 8;
  const int16_t graphH = 200;
  const float barW = (float)graphW / (float)BINS;

  while (true) {
    if (running) {
      deselectOtherSpi();
      for (int n = 0; n < 16; ++n) {
        if (!locked) ch = (ch + 1) % 126;
        writeRegister(NRF24_RF_CH, ch);
        setRxMode();
        delayMicroseconds(60);
        ceLow();
        const int bin = ch * BINS / 126;
        if (carrierDetected()) {
          if (hist[bin] < 120) hist[bin] = (uint8_t)min(120, hist[bin] + 18);
        } else if (hist[bin] > 0) {
          hist[bin]--;
        }
      }
    }

    tft.fillRect(graphX, graphY, graphW, graphH, TFT_BLACK);
    tft.drawRect(graphX, graphY, graphW, graphH, WD_EDGE);
    for (int i = 0; i < BINS; ++i) {
      int16_t h = (int16_t)((hist[i] * (graphH - 4)) / 120);
      int16_t x = graphX + (int16_t)(i * barW);
      int16_t y = graphY + graphH - 2 - h;
      tft.fillRect(x, y, max((int16_t)1, (int16_t)(barW - 1)), h,
                   hist[i] > 70 ? WD_RED : WD_CYAN);
    }
    char line[40];
    snprintf(line, sizeof(line), "CH %u %s", ch, locked ? "LOCK" : "HOP");
    tft.setTextColor(WD_BONE, TFT_BLACK);
    tft.drawString(line, graphX + 2, graphY - 12, 1);

    uint32_t until = millis() + 120;
    while (millis() < until) {
      uint16_t x, y;
      int8_t btn = pollBottomButton(x, y);
      if (btn < 0) { delay(10); continue; }
      waitTouchRelease();
      if (btn == 0) {
        running = !running;
        drawBottomBar(running ? "PAUSE" : "RUN", "CLR", locked ? "HOP" : "LOCK", "BACK");
      } else if (btn == 1) {
        memset(hist, 0, sizeof(hist));
      } else if (btn == 2) {
        locked = !locked;
        drawBottomBar(running ? "PAUSE" : "RUN", "CLR", locked ? "HOP" : "LOCK", "BACK");
      } else if (btn == 3) {
        restoreDefaults();
        return;
      }
      break;
    }
  }
}

void Nrf24Interface::runWlanJammer() {
  TFT_eSPI& tft = display_obj.tft;
  if (!initialized_ && !begin()) {
    drawToolChrome("WLAN JAMMER", "nRF24 not detected");
    drawBottomBar("-", "-", "-", "BACK");
    while (true) {
      uint16_t x, y;
      if (pollBottomButton(x, y) == 3) { waitTouchRelease(); break; }
      delay(20);
    }
    return;
  }

  // WiFi 1..13 map to NRF channel ranges (Evaware tables)
  static const uint8_t WIFI_START[] = {1, 6, 11, 16, 21, 26, 31, 36, 41, 46, 51, 56, 61};
  static const uint8_t WIFI_END[]   = {23, 28, 33, 38, 43, 48, 53, 58, 63, 68, 73, 78, 83};
  int wifiSel = 0; // 0 = ALL
  bool active = false;
  uint8_t hop = 1;

  drawToolChrome("WLAN JAMMER", "CW carrier hop — authorized labs only");
  drawBottomBar("START", "CH+", "ALL", "BACK");

  auto drawStatus = [&]() {
    tft.fillRect(4, 48, tft.width() - 8, 200, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(active ? WD_RED : WD_DIM, TFT_BLACK);
    tft.drawString(active ? "TX ACTIVE" : "IDLE", 10, 60, 2);
    char buf[48];
    if (wifiSel == 0) snprintf(buf, sizeof(buf), "TARGET: ALL WiFi 1-13");
    else snprintf(buf, sizeof(buf), "TARGET: WiFi CH %d", wifiSel);
    tft.setTextColor(WD_BONE, TFT_BLACK);
    tft.drawString(buf, 10, 100, 2);
    snprintf(buf, sizeof(buf), "NRF hop CH: %u", hop);
    tft.drawString(buf, 10, 140, 2);
    tft.setTextColor(WD_AMBER, TFT_BLACK);
    tft.drawString("Hold away from production nets", 10, 180, 1);
  };
  drawStatus();

  while (true) {
    if (active) {
      deselectOtherSpi();
      if (wifiSel == 0) {
        hop++;
        if (hop > 83) hop = 1;
      } else {
        const int idx = wifiSel - 1;
        hop++;
        if (hop > WIFI_END[idx] || hop < WIFI_START[idx]) hop = WIFI_START[idx];
      }
      writeRegister(NRF24_RF_CH, hop);
      // keep carrier: CE stays high; just retune channel
      delayMicroseconds(400);
    }

    static uint32_t lastUi = 0;
    if (millis() - lastUi > 250) {
      lastUi = millis();
      drawStatus();
    }

    uint16_t x, y;
    int8_t btn = pollBottomButton(x, y);
    if (btn >= 0) {
      waitTouchRelease();
      if (btn == 0) {
        active = !active;
        if (active) {
          startConstCarrier(hop);
          drawBottomBar("STOP", "CH+", "ALL", "BACK");
        } else {
          stopConstCarrier();
          restoreDefaults();
          drawBottomBar("START", "CH+", "ALL", "BACK");
        }
      } else if (btn == 1) {
        wifiSel = (wifiSel % 13) + 1;
      } else if (btn == 2) {
        wifiSel = 0;
      } else if (btn == 3) {
        active = false;
        stopConstCarrier();
        restoreDefaults();
        return;
      }
      drawStatus();
    } else {
      delay(active ? 0 : 15);
    }
  }
}

void Nrf24Interface::runProtoKill() {
  TFT_eSPI& tft = display_obj.tft;
  if (!initialized_ && !begin()) {
    drawToolChrome("PROTO KILL", "nRF24 not detected");
    drawBottomBar("-", "-", "-", "BACK");
    while (true) {
      uint16_t x, y;
      if (pollBottomButton(x, y) == 3) { waitTouchRelease(); break; }
      delay(20);
    }
    return;
  }

  bool active = false;
  uint8_t hop = 0;
  drawToolChrome("PROTO KILL", "Broadband 2.4GHz CW hop 0-125");
  drawBottomBar("START", "-", "-", "BACK");

  auto drawStatus = [&]() {
    tft.fillRect(4, 48, tft.width() - 8, 200, TFT_BLACK);
    tft.setTextColor(active ? WD_RED : WD_DIM, TFT_BLACK);
    tft.drawString(active ? "KILL ACTIVE" : "IDLE", 10, 70, 2);
    char buf[40];
    snprintf(buf, sizeof(buf), "NRF CH %u / 125", hop);
    tft.setTextColor(WD_BONE, TFT_BLACK);
    tft.drawString(buf, 10, 120, 2);
  };
  drawStatus();

  while (true) {
    if (active) {
      hop = (hop + 1) % 126;
      writeRegister(NRF24_RF_CH, hop);
      delayMicroseconds(350);
    }
    static uint32_t lastUi = 0;
    if (millis() - lastUi > 200) {
      lastUi = millis();
      drawStatus();
    }
    uint16_t x, y;
    int8_t btn = pollBottomButton(x, y);
    if (btn >= 0) {
      waitTouchRelease();
      if (btn == 0) {
        active = !active;
        if (active) {
          startConstCarrier(hop);
          drawBottomBar("STOP", "-", "-", "BACK");
        } else {
          stopConstCarrier();
          restoreDefaults();
          drawBottomBar("START", "-", "-", "BACK");
        }
      } else if (btn == 3) {
        stopConstCarrier();
        restoreDefaults();
        return;
      }
      drawStatus();
    } else {
      delay(active ? 0 : 15);
    }
  }
}

void Nrf24Interface::runPromiscuousSniffer() {
  TFT_eSPI& tft = display_obj.tft;
  if (!initialized_ && !begin()) {
    drawToolChrome("NRF SNIFFER", "nRF24 not detected");
    drawBottomBar("-", "-", "-", "BACK");
    while (true) {
      uint16_t x, y;
      if (pollBottomButton(x, y) == 3) { waitTouchRelease(); break; }
      delay(20);
    }
    return;
  }

  struct Hit {
    uint8_t addr[5];
    uint8_t hits;
    uint8_t ch;
  };
  constexpr int MAX_HITS = 24;
  Hit hits[MAX_HITS];
  int hitCount = 0;
  uint32_t frames = 0;
  uint8_t ch = 0;
  bool running = true;
  bool locked = false;

  auto isNoise = [](const uint8_t* b, int n) -> bool {
    if (n < 5) return true;
    bool allSame = true;
    for (int i = 1; i < n; ++i) if (b[i] != b[0]) { allSame = false; break; }
    if (allSame) return true;
    int alt = 0;
    for (int i = 0; i < min(n, 8); ++i)
      if (b[i] == 0xAA || b[i] == 0x55) alt++;
    return alt >= 7;
  };

  auto findOrAdd = [&](const uint8_t* addr) -> int {
    for (int i = 0; i < hitCount; ++i)
      if (memcmp(hits[i].addr, addr, 5) == 0) return i;
    if (hitCount < MAX_HITS) {
      int i = hitCount++;
      memcpy(hits[i].addr, addr, 5);
      hits[i].hits = 0;
      hits[i].ch = 0;
      return i;
    }
    memmove(&hits[0], &hits[1], sizeof(Hit) * (MAX_HITS - 1));
    int i = MAX_HITS - 1;
    memcpy(hits[i].addr, addr, 5);
    hits[i].hits = 0;
    hits[i].ch = 0;
    return i;
  };

  // Goodspeed promiscuous setup
  deselectOtherSpi();
  ceLow();
  powerUp();
  writeRegister(NRF24_SETUP_AW, 0x00); // illegal 2-byte width
  writeRegister(NRF24_EN_AA, 0x00);
  writeRegister(NRF24_EN_RXADDR, 0x03);
  writeRegister(NRF24_RF_SETUP, 0x09);
  writeRegister(NRF24_RX_PW_P0, 32);
  writeRegister(NRF24_RX_PW_P1, 32);
  const uint8_t a0[] = {0xAA, 0x55};
  const uint8_t a1[] = {0x55, 0xAA};
  writeRegisterMulti(NRF24_RX_ADDR_P0, a0, 2);
  writeRegisterMulti(NRF24_RX_ADDR_P1, a1, 2);
  flushRx();
  writeRegister(NRF24_STATUS, 0x70);
  writeRegister(NRF24_CONFIG, 0x03); // PWR_UP | PRIM_RX, CRC off
  delayMicroseconds(1500);
  ceHigh();

  drawToolChrome("NRF SNIFFER", "Promiscuous Goodspeed RX");
  drawBottomBar("PAUSE", "LOCK", "CLR", "BACK");

  while (true) {
    if (running) {
      for (int passes = 0; passes < 40; ++passes) {
        if (!locked) {
          ch++;
          if (ch > 125) ch = 0;
        }
        writeRegister(NRF24_RF_CH, ch);
        delayMicroseconds(200);
        for (int r = 0; r < 3; ++r) {
          uint8_t st = readRegister(NRF24_STATUS);
          if (!(st & 0x40)) break;
          uint8_t buf[32] = {};
          readPayload(buf, 32);
          writeRegister(NRF24_STATUS, 0x70);
          frames++;
          if (isNoise(buf, 32)) continue;
          int idx = findOrAdd(buf);
          hits[idx].hits = (uint8_t)min(255, hits[idx].hits + 1);
          hits[idx].ch = ch;
        }
      }
    }

    tft.fillRect(4, 48, tft.width() - 8, 200, TFT_BLACK);
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "CH:%u FR:%lu DEV:%d %s",
             ch, (unsigned long)frames, hitCount, locked ? "LOCK" : "HOP");
    tft.setTextColor(WD_CYAN, TFT_BLACK);
    tft.drawString(hdr, 8, 52, 1);
    tft.setTextColor(WD_BONE, TFT_BLACK);
    const int show = min(hitCount, 10);
    for (int i = 0; i < show; ++i) {
      char line[48];
      snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X  x%u ch%u",
               hits[i].addr[0], hits[i].addr[1], hits[i].addr[2],
               hits[i].addr[3], hits[i].addr[4], hits[i].hits, hits[i].ch);
      tft.drawString(line, 8, 70 + i * 16, 1);
    }
    if (hitCount == 0) {
      tft.setTextColor(WD_DIM, TFT_BLACK);
      tft.drawString("Listening for 2.4GHz addresses...", 8, 90, 1);
    }

    uint32_t until = millis() + 150;
    while (millis() < until) {
      uint16_t x, y;
      int8_t btn = pollBottomButton(x, y);
      if (btn < 0) { delay(8); continue; }
      waitTouchRelease();
      if (btn == 0) {
        running = !running;
        drawBottomBar(running ? "PAUSE" : "RUN", locked ? "HOP" : "LOCK", "CLR", "BACK");
      } else if (btn == 1) {
        locked = !locked;
        drawBottomBar(running ? "PAUSE" : "RUN", locked ? "HOP" : "LOCK", "CLR", "BACK");
      } else if (btn == 2) {
        hitCount = 0;
        frames = 0;
      } else if (btn == 3) {
        ceLow();
        writeRegister(NRF24_SETUP_AW, 0x03); // CRITICAL restore
        restoreDefaults();
        return;
      }
      break;
    }
  }
}

void Nrf24Interface::runMouseJack() {
  TFT_eSPI& tft = display_obj.tft;
  if (!initialized_ && !begin()) {
    drawToolChrome("MOUSEJACK", "nRF24 not detected");
    drawBottomBar("-", "-", "-", "BACK");
    while (true) {
      uint16_t x, y;
      if (pollBottomButton(x, y) == 3) { waitTouchRelease(); break; }
      delay(20);
    }
    return;
  }

  struct Dev {
    uint8_t addr[5];
    uint8_t hits;
    uint8_t ch;
    char tag[12];
  };
  constexpr int MAX_DEV = 16;
  Dev devs[MAX_DEV];
  int devCount = 0;
  uint8_t ch = 0;
  bool scanning = true;
  int selected = 0;
  uint32_t injects = 0;

  auto classify = [](const uint8_t* a) -> const char* {
    // Heuristic vendor nibble tags (not authoritative)
    if (a[0] == 0x00 && a[1] == 0x5F) return "LOGITECH?";
    if (a[0] == 0xCD) return "MSFT?";
    return "UNKNOWN";
  };

  // Promiscuous-ish RX for dongle discovery
  deselectOtherSpi();
  ceLow();
  powerUp();
  writeRegister(NRF24_SETUP_AW, 0x03);
  writeRegister(NRF24_EN_AA, 0x00);
  writeRegister(NRF24_EN_RXADDR, 0x01);
  writeRegister(NRF24_RF_SETUP, 0x07); // 1Mbps max power-ish
  writeRegister(NRF24_RX_PW_P0, 32);
  writeRegister(NRF24_SETUP_AW, 0x00);
  const uint8_t a0[] = {0xAA, 0x00};
  writeRegisterMulti(NRF24_RX_ADDR_P0, a0, 2);
  flushRx();
  writeRegister(NRF24_CONFIG, 0x03);
  delayMicroseconds(1500);
  ceHigh();

  drawToolChrome("MOUSEJACK", "Discover + inject demo (lab only)");
  drawBottomBar("PAUSE", "SEL+", "INJECT", "BACK");

  while (true) {
    if (scanning) {
      for (int p = 0; p < 30; ++p) {
        ch++;
        if (ch > 85) ch = 0;
        writeRegister(NRF24_RF_CH, ch);
        delayMicroseconds(250);
        uint8_t st = readRegister(NRF24_STATUS);
        if (st & 0x40) {
          uint8_t buf[32] = {};
          readPayload(buf, 32);
          writeRegister(NRF24_STATUS, 0x70);
          // skip pure noise
          int zeros = 0;
          for (int i = 0; i < 5; ++i) if (buf[i] == 0 || buf[i] == 0xFF) zeros++;
          if (zeros >= 4) continue;
          int found = -1;
          for (int i = 0; i < devCount; ++i)
            if (memcmp(devs[i].addr, buf, 5) == 0) { found = i; break; }
          if (found < 0 && devCount < MAX_DEV) {
            found = devCount++;
            memcpy(devs[found].addr, buf, 5);
            devs[found].hits = 0;
            strncpy(devs[found].tag, classify(buf), sizeof(devs[found].tag) - 1);
            devs[found].tag[sizeof(devs[found].tag) - 1] = 0;
          }
          if (found >= 0) {
            devs[found].hits = (uint8_t)min(255, devs[found].hits + 1);
            devs[found].ch = ch;
          }
        }
      }
    }

    tft.fillRect(4, 48, tft.width() - 8, 200, TFT_BLACK);
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "CH:%u DEVS:%d INJ:%lu %s",
             ch, devCount, (unsigned long)injects, scanning ? "SCAN" : "HOLD");
    tft.setTextColor(WD_CYAN, TFT_BLACK);
    tft.drawString(hdr, 8, 52, 1);
    if (devCount == 0) {
      tft.setTextColor(WD_DIM, TFT_BLACK);
      tft.drawString("Scanning for wireless HID...", 8, 90, 1);
    }
    const int show = min(devCount, 9);
    for (int i = 0; i < show; ++i) {
      char line[56];
      snprintf(line, sizeof(line), "%c %02X%02X%02X%02X%02X %s x%u",
               (i == selected) ? '>' : ' ',
               devs[i].addr[0], devs[i].addr[1], devs[i].addr[2],
               devs[i].addr[3], devs[i].addr[4],
               devs[i].tag, devs[i].hits);
      tft.setTextColor(i == selected ? WD_AMBER : WD_BONE, TFT_BLACK);
      tft.drawString(line, 6, 70 + i * 16, 1);
    }

    uint32_t until = millis() + 160;
    while (millis() < until) {
      uint16_t x, y;
      int8_t btn = pollBottomButton(x, y);
      if (btn < 0) { delay(8); continue; }
      waitTouchRelease();
      if (btn == 0) {
        scanning = !scanning;
        drawBottomBar(scanning ? "PAUSE" : "SCAN", "SEL+", "INJECT", "BACK");
      } else if (btn == 1) {
        if (devCount > 0) selected = (selected + 1) % devCount;
      } else if (btn == 2) {
        if (devCount > 0) {
          // Demo inject: briefly leave promiscuous, blast noise payload to addr
          ceLow();
          writeRegister(NRF24_SETUP_AW, 0x03);
          writeRegisterMulti(NRF24_TX_ADDR, devs[selected].addr, 5);
          writeRegisterMulti(NRF24_RX_ADDR_P0, devs[selected].addr, 5);
          writeRegister(NRF24_RF_CH, devs[selected].ch);
          writeRegister(NRF24_EN_AA, 0x00);
          writeRegister(NRF24_SETUP_RETR, 0x00);
          setTxMode();
          uint8_t payload[16];
          memset(payload, 0x00, sizeof(payload));
          payload[0] = 0x00; // HID-ish placeholder
          payload[1] = 0x04; // 'a' keycode demo
          for (int n = 0; n < 8; ++n) {
            writePayload(payload, sizeof(payload));
            ceHigh();
            delayMicroseconds(20);
            ceLow();
            delayMicroseconds(400);
          }
          injects++;
          // restore sniffer mode
          writeRegister(NRF24_SETUP_AW, 0x00);
          writeRegisterMulti(NRF24_RX_ADDR_P0, a0, 2);
          writeRegister(NRF24_CONFIG, 0x03);
          delayMicroseconds(500);
          ceHigh();
        }
      } else if (btn == 3) {
        ceLow();
        writeRegister(NRF24_SETUP_AW, 0x03);
        restoreDefaults();
        return;
      }
      break;
    }
  }
}

Nrf24Interface nrf24_obj;

#endif
