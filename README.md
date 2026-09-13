# M5SHARK R150

M5SHARK is a handheld ESP32 wireless security-testing firmware with Wi-Fi,
Bluetooth, GPS, SD, touch UI, web control, and optional external radio modules.

- Product: https://m5shark.com/products/m5shark-marauder-v8
- Support MARAWDA: https://www.buymeacoffee.com/marawda
- License: GPL-3.0, see [LICENSE](LICENSE)

## Main Features

- Wi-Fi surveys, wardriving, channel analysis, packet monitoring, sniffers,
  PCAP logging, Guardian monitoring, and authorized testing tools
- BLE scanners, analyzers, trackers, RSSI monitoring, and BLE BadUSB
- GPS data, GPX tracking, POI logging, and wardrive coordinates
- nRF24L01+, CC1101, and PN532 diagnostic/toolbox support
- nRF24 carrier heatmap and cross-module RF status/sweep menu
- Guardian Baseline for trusted AP verification and rogue-AP alerts
- Autonomous Sentinel background alert logging to `/shark/sentinel.log`
- Bjorn CYD-style host/service reconnaissance launcher
- SHARK Field Operations menu with surveys, recon, RF tools, self-test, and reports
- Ten themes: Matrix, Watch Dogs, Cyber 2077, Spider, Aurora, Crimson, Sunset,
  Violet, Ghost, and Neon
- Local web dashboard with Wi-Fi tools, themes, Guardian status, and SD files

## Supported Displays

The default target is the M5SHARK v8 ESP32-C5 board.

Optional ports:

- `MARAUDER_HOSYOND_35`: Hosyond ESP32, 320x480 ST7796U, resistive touch
- `MARAUDER_WAVESHARE_C5_28`: Waveshare ESP32-C5, 240x320 ST7789, CST3530
  capacitive touch, CH32V003 I/O expander

Waveshare pins: LCD MOSI 7, SCLK 6, CS 10, DC 9; touch SDA 0, SCL 1,
interrupt 5; SD CS 23.

## Automatic Builds

Linux:

```bash
./build-port.sh M5SHARK_V8
./build-port.sh HOSYOND_35
./build-port.sh WAVESHARE_C5_28
```

PowerShell:

```powershell
.\build-port.ps1 -Target M5SHARK_V8
.\build-port.ps1 -Target HOSYOND_35
.\build-port.ps1 -Target WAVESHARE_C5_28
```

The TFT_eSPI `User_Setup.h` driver must match the target: ST7796 for Hosyond
or ST7789 for Waveshare.

## Build Requirements

- Arduino ESP32 core 3.3.4
- NimBLE-Arduino 2.5.1
- TFT_eSPI
- ArduinoJson, MicroNMEA, LinkedList, ESPAsyncWebServer, and AsyncTCP
- Adafruit MAX1704X and BusIO for the battery path
- ESP32Ping and XPT2046_Touchscreen where applicable

Normal M5SHARK build:

```powershell
powershell -ExecutionPolicy Bypass -File build-shark-v8.ps1
```

## Project Layout

- `m5shark/m5shark.ino`: boot and main loop
- `m5shark/configs.h`: board flags, pins, display geometry, and features
- `m5shark/WiFiScan.*`: Wi-Fi/BLE engine and scan state
- `m5shark/MenuFunctions.*`: menus, screens, and touch actions
- `m5shark/SharkWeb.*`: local web dashboard and APIs
- `m5shark/SharkSentinel.*`: persistent passive alert service
- `m5shark/Nrf24Interface.*`: nRF24 diagnostics and carrier heatmap
- `m5shark/CC1101Interface.*`: CC1101 diagnostics and receive tools
- `m5shark/Pn532Interface.*`: PN532 diagnostics and UID reads
- `m5shark/cst3530.h`: Waveshare capacitive touch support

## Credits

M5SHARK is informed by and builds on open-source work from:

- [ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder)
- [HaleHound-CYD](https://github.com/JesseCHale/HaleHound-CYD)
- [Bjorn](https://github.com/infinition/Bjorn)
- [MARAWDA cyd_bjorn-port](https://github.com/MARAWDA/cyd_bjorn-port)
- [RF-Clown](https://github.com/MARAWDA/RF-Clown)
- [nRFBox](https://github.com/MARAWDA/nRFBox)
- [Waveshare ESP32-C5 Touch LCD examples](https://github.com/waveshareteam/ESP32-C5-Touch-LCD-2.8)

Meshtastic is a future SX1262/LoRa direction. nRF24L01+ is not
Meshtastic-compatible.

## Legal

Use this firmware only on networks, devices, and systems you own or are
explicitly authorized to test. Follow all local laws.

## Support

Support continued MARAWDA development:

https://www.buymeacoffee.com/marawda
