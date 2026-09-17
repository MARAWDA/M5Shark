#include "SDInterface.h"
#include "lang_var.h"

#ifdef HAS_C5_SD
  SDInterface::SDInterface(SPIClass* spi, int cs)
    : _spi(spi), _cs(cs) {}
#endif

bool SDInterface::initSD() {
  #ifdef HAS_SD
    String display_string = "";

    #ifdef KIT
      pinMode(SD_DET, INPUT);
      if (digitalRead(SD_DET) != LOW) {
        this->supported = false;
        return false;
      }
    #endif

    #if defined(SD_CS) && (SD_CS >= 0)
      pinMode(SD_CS, OUTPUT);
      digitalWrite(SD_CS, HIGH);
    #endif

    delay(10);
    #if (defined(MARAUDER_M5STICKC)) || (defined(HAS_CYD_TOUCH)) || (defined(HAS_SEPARATE_SD)) || (defined(MARAUDER_CARDPUTER)) || (defined(MARAUDER_CARDPUTER_ADV))
      #if defined(MARAUDER_M5STICKC)
        enum { SPI_SCK = 0, SPI_MISO = 36, SPI_MOSI = 26 };
      #elif defined(HAS_CYD_TOUCH) || defined(MARAUDER_CARDPUTER) || defined(MARAUDER_CARDPUTER_ADV) || defined(HAS_SEPARATE_SD)
        enum { SPI_SCK = SD_SCK, SPI_MISO = SD_MISO, SPI_MOSI = SD_MOSI };
      #else
        enum { SPI_SCK = 0, SPI_MISO = 36, SPI_MOSI = 26 };
      #endif

      // Deselect TFT/touch without reconfiguring pin modes (preserves TFT_eSPI).
      #if defined(TFT_CS)
        digitalWrite(TFT_CS, HIGH);
      #endif
      #if defined(TOUCH_CS) && (TOUCH_CS >= 0)
        digitalWrite(TOUCH_CS, HIGH);
      #endif

      #if !defined(MARAUDER_CARDPUTER) && !defined(MARAUDER_CARDPUTER_ADV)
        // CRITICAL: TFT_eSPI (display + XPT2046 via TOUCH_CS) defaults to VSPI.
        // SD must use the other host (HSPI) or touch/display SPI is stolen and
        // the panel stops responding to touch after SD init.
        if (this->spiExt == nullptr) this->spiExt = new SPIClass(HSPI);
      #else
        if (this->spiExt == nullptr) this->spiExt = new SPIClass(FSPI);
      #endif

      Serial.printf("[SD] external SPI SCK=%d MISO=%d MOSI=%d CS=%d\n",
                    (int)SPI_SCK, (int)SPI_MISO, (int)SPI_MOSI, (int)SD_CS);

      // Card may need settle after hot-plug / leave PC reader.
      delay(80);

      bool mounted = false;
      // Slow-first: after reinsert many cards only answer at 100–400 kHz.
      const uint32_t rates[] = {
          100000u, 400000u, 1000000u, 2000000u, 4000000u, 8000000u, 10000000u
      };
      uint32_t try_hz[8];
      size_t ntry = 0;
      if (this->mount_hz) try_hz[ntry++] = this->mount_hz;
      for (size_t i = 0; i < sizeof(rates)/sizeof(rates[0]); i++) {
        bool dup = false;
        for (size_t j = 0; j < ntry; j++) if (try_hz[j] == rates[i]) dup = true;
        if (!dup) try_hz[ntry++] = rates[i];
      }

      // Two full passes: first soft, second with SPI bus re-init + CS toggle.
      for (int pass = 0; pass < 2 && !mounted; pass++) {
        this->spiExt->end();
        delay(10);
        this->spiExt->begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
        #if defined(SD_CS) && (SD_CS >= 0)
          pinMode(SD_CS, OUTPUT);
          digitalWrite(SD_CS, HIGH);
          delay(5);
          digitalWrite(SD_CS, LOW);
          delay(2);
          digitalWrite(SD_CS, HIGH);
          delay(20);
        #endif
        if (pass) delay(120);  // extra settle on second pass

        for (size_t i = 0; i < ntry && !mounted; i++) {
          const uint32_t hz = try_hz[i];
          SD.end();
          delay(15);
          #if defined(SD_CS) && (SD_CS >= 0)
            digitalWrite(SD_CS, HIGH);
          #endif
          mounted = SD.begin(SD_CS, *(this->spiExt), hz);
          if (!mounted) {
            Serial.printf("[SD] begin fail pass%u @%lu Hz\n",
                          (unsigned)pass, (unsigned long)hz);
            delay(25);
          } else {
            this->mount_hz = hz;
            Serial.printf("[SD] mounted @%lu Hz (pass %u)\n",
                          (unsigned long)hz, (unsigned)pass);
          }
        }
      }
      if (!mounted) {
    #elif defined(HAS_C5_SD)
      if (!SD.begin(SD_CS, *_spi)) {
    #else
      if (!SD.begin(SD_CS)) {
    #endif
      Serial.println(F("Failed to mount SD Card"));
      this->supported = false;
      return false;
    }
    else {
      this->supported = true;
      this->cardType = SD.cardType();
      this->cardSizeMB = SD.cardSize() / (1024 * 1024);

      if (this->supported) {
        const int NUM_DIGITS = log10(this->cardSizeMB) + 1;
        char sz[NUM_DIGITS + 1];
        sz[NUM_DIGITS] = 0;
        for (size_t i = NUM_DIGITS; i--; this->cardSizeMB /= 10) {
          sz[i] = '0' + (this->cardSizeMB % 10);
          display_string.concat((String)sz[i]);
        }
        this->card_sz = sz;
      }

      // Best-effort; ignore mkdir failure (RO cards still mount for dual-boot reads).
      if (!SD.exists("/SCRIPTS")) {
        SD.mkdir("/SCRIPTS");
      }

      if (this->sd_files == nullptr) this->sd_files = new LinkedList<String>();
      Serial.printf("[SD] OK type=%u size_str=%s\n", (unsigned)this->cardType, this->card_sz.c_str());
      return true;
    }

  #else
    return false;
  #endif
}

bool SDInterface::isMounted() {
  #ifdef HAS_SD
    // Trust boot-time mount. Live open("/") probes + pinMode(TFT_CS) thrash the
    // display SPI and falsely report "no SD" while the card is still present.
    return this->supported;
  #else
    return false;
  #endif
}

bool SDInterface::ensureMounted() {
  #ifdef HAS_SD
    if (this->supported) {
      // Soft existence check without touching TFT pins.
      if (SD.cardType() != CARD_NONE) return true;
      // cardType flaky — still try open once
      File root = SD.open("/");
      if (root) { root.close(); return true; }
      // Fall through to remount
      this->supported = false;
    }
    return this->remountSD();
  #else
    return false;
  #endif
}

bool SDInterface::remountSD() {
  #ifdef HAS_SD
    if (this->supported) {
      // Verify still alive
      if (SD.cardType() != CARD_NONE) {
        File r = SD.open("/");
        if (r) { r.close(); Serial.println(F("[SD] already mounted")); return true; }
      }
      this->supported = false;
    }
    Serial.println(F("[SD] remount..."));
    #if defined(SD_CS) && (SD_CS >= 0)
      pinMode(SD_CS, OUTPUT);
      digitalWrite(SD_CS, HIGH);
    #endif
    // Do NOT pinMode(TFT_CS/TOUCH_CS) here — separate HSPI SD bus; stomping
    // those pins breaks TFT_eSPI touch after dual-boot probes.
    SD.end();
    delay(80);
    #if (defined(MARAUDER_M5STICKC)) || (defined(HAS_CYD_TOUCH)) || (defined(HAS_SEPARATE_SD)) || (defined(MARAUDER_CARDPUTER)) || (defined(MARAUDER_CARDPUTER_ADV))
      if (this->spiExt != nullptr) {
        #if defined(HAS_CYD_TOUCH) || defined(MARAUDER_CARDPUTER) || defined(MARAUDER_CARDPUTER_ADV) || defined(HAS_SEPARATE_SD)
          this->spiExt->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
        #elif defined(MARAUDER_M5STICKC)
          this->spiExt->begin(0, 36, 26, SD_CS);
        #endif
      }
    #endif
    return this->initSD();
  #else
    return false;
  #endif
}

File SDInterface::getFile(String path) {
  if (this->supported) {
    File file = SD.open(path, FILE_READ);
    return file;
  }
  return File();
}

bool SDInterface::removeFile(String file_path) {
  if (SD.remove(file_path))
    return true;
  else
    return false;
}

void SDInterface::listDirToLinkedList(LinkedList<String>* file_names, String str_dir, String ext) {
  if (this->supported) {
    File dir = SD.open(str_dir);
    while (true)
    {
      File entry = dir.openNextFile();
      if (!entry)
      {
        break;
      }

      if (entry.isDirectory())
        continue;

      String file_name = entry.name();
      if (ext != "") {
        if (file_name.endsWith(ext)) {
          file_names->add(file_name);
        }
      }
      else
        file_names->add(file_name);
    }
  }
}

void SDInterface::listDir(String str_dir){
  if (this->supported) {
    File dir = SD.open(str_dir);
    while (true)
    {
      File entry = dir.openNextFile();
      if (! entry)
      {
        break;
      }
      //for (uint8_t i = 0; i < numTabs; i++)
      //{
      //  Serial.print('\t');
      //}
      Serial.print(entry.name());
      Serial.print("\t");
      Serial.println(entry.size());
      entry.close();
    }
  }
}

void SDInterface::runUpdate(String file_name) {
  if (file_name == "")
    file_name = "/update.bin";

  #ifdef HAS_SCREEN
    display_obj.tft.setTextWrap(false);
    display_obj.tft.setFreeFont(NULL);
    display_obj.tft.setCursor(0, TFT_HEIGHT / 3);
    display_obj.tft.setTextSize(1);
    display_obj.tft.setTextColor(TFT_WHITE);
  
    display_obj.tft.println("Opening " + file_name + "...");
  #endif

  File updateBin = SD.open(file_name);

  if (updateBin) {
    if(updateBin.isDirectory()){
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_RED);
        display_obj.tft.println(F(text_table2[0]));
      #endif
      Serial.print(F("Error, could not find \""));
      Serial.print(file_name);
      Serial.println(F("\""));
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_WHITE);
      #endif
      updateBin.close();
      return;
    }

    size_t updateSize = updateBin.size();

    bool updateSucceeded = false;
    if (updateSize > 0) {
      #ifdef HAS_SCREEN
        display_obj.tft.println(F(text_table2[1]));
      #endif
      Serial.println(F("Starting update over SD. Please wait..."));
      updateSucceeded = this->performUpdate(updateBin, updateSize);
    }
    else {
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_RED);
        display_obj.tft.println(F(text_table2[2]));
      #endif
      Serial.println(F("Error, file is empty"));
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_WHITE);
      #endif
      return;
    }

    updateBin.close();
    if (!updateSucceeded) {
      #ifdef HAS_SCREEN
        display_obj.tft.setTextColor(TFT_RED);
        display_obj.tft.println(F("Update failed; firmware unchanged"));
        display_obj.tft.setTextColor(TFT_WHITE);
      #endif
      Serial.println(F("SD update failed; keeping current firmware"));
      return;
    }

    #ifdef HAS_SCREEN
      display_obj.tft.println(F(text_table2[3]));
    #endif
    ESP.restart();
  }
  else {
    #ifdef HAS_SCREEN
      display_obj.tft.setTextColor(TFT_RED);
      display_obj.tft.println(F(text_table2[4]));
    #endif
    Serial.println(F("Could not load update.bin from sd root"));
    #ifdef HAS_SCREEN
      display_obj.tft.setTextColor(TFT_WHITE);
    #endif
  }
}

bool SDInterface::performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    #ifdef HAS_SCREEN
      display_obj.tft.println(text_table2[5] + String(updateSize));
      display_obj.tft.println(F(text_table2[6]));
      // Progress bar frame; the fill redraws as the write advances.
      display_obj.tft.drawRoundRect(10, 200, 220, 20, 3, TFT_CYAN);
      display_obj.tft.fillRect(12, 202, 216, 16, TFT_BLACK);
    #endif
    // Chunked write with live progress. Update.writeStream() is a single
    // blocking call - the ~3.3 MB image left the screen frozen for a
    // minute or more with no sign of life.
    size_t written = 0;
    uint8_t updateBuf[4096];
    uint8_t lastPct = 255;
    while (written < updateSize) {
      size_t avail = updateSource.readBytes((char*)updateBuf, sizeof(updateBuf));
      if (avail == 0)
        break;   // stream ended early; the size check below reports it
      size_t w = Update.write(updateBuf, avail);
      written += w;
      if (w != avail)
        break;
      const uint8_t pct = (uint8_t)(written * 100 / updateSize);
      if (pct != lastPct) {
        lastPct = pct;
        #ifdef HAS_SCREEN
          display_obj.tft.fillRect(12, 202, (uint16_t)(216 * written / updateSize), 16, TFT_CYAN);
          display_obj.tft.setTextDatum(MC_DATUM);
          display_obj.tft.setTextColor(TFT_WHITE, TFT_CYAN);
          display_obj.tft.drawString(String(pct) + "%", 120, 210, 2);
          display_obj.tft.setTextDatum(TL_DATUM);
        #endif
        if (pct % 10 == 0) {
          Serial.print(F("SD update progress: "));
          Serial.print(pct);
          Serial.println(F("%"));
        }
      }
    }
    if (written == updateSize) {
      #ifdef HAS_SCREEN
        display_obj.tft.println(text_table2[7] + String(written) + text_table2[10]);
      #endif
      Serial.print(F("Written : "));
      Serial.print(written);
      Serial.println(F(" successfully"));
    }
    else {
      #ifdef HAS_SCREEN
        display_obj.tft.println(text_table2[8] + String(written) + "/" + String(updateSize) + text_table2[9]);
      #endif
      Serial.print(F("Written only : "));
      Serial.print(written);
      Serial.print(F("/"));
      Serial.print(updateSize);
      Serial.println(F(". Retry?"));
    }
    if (Update.end()) {
      if (Update.isFinished()) {
        return written == updateSize;
      }
      else {
        #ifdef HAS_SCREEN
          display_obj.tft.setTextColor(TFT_RED);
          display_obj.tft.println(text_table2[12]);
        #endif
        Serial.println(F("Update not finished? Something went wrong!"));
        #ifdef HAS_SCREEN
          display_obj.tft.setTextColor(TFT_WHITE);
        #endif
      }
    }
    else {
      #ifdef HAS_SCREEN
        display_obj.tft.println(text_table2[13] + String(Update.getError()));
      #endif
      Serial.print(F("Error Occurred. Error #: "));
      Serial.println(Update.getError());
    }
    return false;
  }
  else
  {
    #ifdef HAS_SCREEN
      display_obj.tft.println(text_table2[14]);
    #endif
    Serial.println(F("Not enough space to begin OTA"));
    return false;
  }
}
