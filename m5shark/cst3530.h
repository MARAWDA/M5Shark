#pragma once

#if defined(HAS_CAP_TOUCH) && defined(HAS_CST3530)

#include <Arduino.h>
#include <Wire.h>
#include <string.h>

// Waveshare's CH32V003 expander controls LCD reset, touch reset, and backlight.
#define CST3530_ADDR 0x58
#define CST3530_BOARD_IO_ADDR 0x24
#define CST3530_SDA_PIN 0
#define CST3530_SCL_PIN 1
#define CST3530_INT_PIN 5
#define CST3530_IO_MODE_REG 0x02
#define CST3530_IO_OUTPUT_REG 0x03
#define CST3530_IO_PWM_REG 0x05
#define CST3530_TOUCH_RESET_IO 0
#define CST3530_LCD_RESET_IO 1
#define CST3530_BACKLIGHT_IO 3
#define CST3530_DATA_REG 0xD0070000UL
#define CST3530_NEXT_REG 0xD0070900UL
#define CST3530_END_REG 0xD00002ABUL
#define CST3530_MAX_POINTS 5

struct Cst3530Point {
  uint16_t x;
  uint16_t y;
  uint16_t strength;
};

static Cst3530Point cst3530_points[CST3530_MAX_POINTS] = {};
static uint8_t cst3530_point_count = 0;
static uint8_t cst3530_io_value = 0;
static bool cst3530_io_ready = false;

static bool cst3530_io_write(uint8_t reg, const uint8_t* data, size_t len) {
  Wire.beginTransmission(CST3530_BOARD_IO_ADDR);
  Wire.write(reg);
  if (data && len) Wire.write(data, len);
  return Wire.endTransmission() == 0;
}

static bool cst3530_io_write8(uint8_t reg, uint8_t value) {
  return cst3530_io_write(reg, &value, 1);
}

static bool cst3530_board_init() {
  Wire.begin(CST3530_SDA_PIN, CST3530_SCL_PIN, 400000U);
  cst3530_io_value = (1U << CST3530_TOUCH_RESET_IO) |
                     (1U << CST3530_LCD_RESET_IO) |
                     (1U << CST3530_BACKLIGHT_IO);
  if (!cst3530_io_write8(CST3530_IO_MODE_REG, 0xFF)) return false;
  cst3530_io_ready = cst3530_io_write8(CST3530_IO_OUTPUT_REG, cst3530_io_value);
  return cst3530_io_ready;
}

static bool cst3530_io_set(uint8_t pin, bool level) {
  if (!cst3530_io_ready && !cst3530_board_init()) return false;
  if (level) cst3530_io_value |= (1U << pin);
  else cst3530_io_value &= ~(1U << pin);
  return cst3530_io_write8(CST3530_IO_OUTPUT_REG, cst3530_io_value);
}

static void cst3530_lcd_reset() {
  cst3530_io_set(CST3530_LCD_RESET_IO, false);
  delay(50);
  cst3530_io_set(CST3530_LCD_RESET_IO, true);
  delay(120);
}

static void cst3530_touch_reset() {
  cst3530_io_set(CST3530_TOUCH_RESET_IO, false);
  delay(100);
  cst3530_io_set(CST3530_TOUCH_RESET_IO, true);
  delay(500);
}

static bool cst3530_read_register(uint32_t reg, uint8_t* data, uint8_t len) {
  Wire.beginTransmission(CST3530_ADDR);
  Wire.write((uint8_t)(reg >> 24));
  Wire.write((uint8_t)(reg >> 16));
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)CST3530_ADDR, (int)len) != len) return false;
  for (uint8_t i = 0; i < len; ++i) data[i] = Wire.read();
  return true;
}

static void cst3530_end_read() {
  Wire.beginTransmission(CST3530_ADDR);
  Wire.write((uint8_t)(CST3530_END_REG >> 24));
  Wire.write((uint8_t)(CST3530_END_REG >> 16));
  Wire.write((uint8_t)(CST3530_END_REG >> 8));
  Wire.write((uint8_t)CST3530_END_REG);
  Wire.endTransmission(true);
}

static bool cst3530_init() {
  if (!cst3530_io_ready && !cst3530_board_init()) return false;
  cst3530_touch_reset();
  pinMode(CST3530_INT_PIN, INPUT);
  Wire.beginTransmission(CST3530_ADDR);
  return Wire.endTransmission(true) == 0;
}

static bool cst3530_read() {
  uint8_t buf[9] = {};
  cst3530_point_count = 0;
  if (!cst3530_read_register(CST3530_DATA_REG, buf, sizeof(buf))) return false;
  const uint8_t count = buf[3] & 0x0F;
  if (!count || count > CST3530_MAX_POINTS || !(buf[8] & 0xF0)) {
    cst3530_end_read();
    return false;
  }

  uint8_t extra[20] = {};
  if (count > 1 && !cst3530_read_register(CST3530_NEXT_REG, extra, (count - 1) * 5)) {
    cst3530_end_read();
    return false;
  }
  uint8_t all[25] = {};
  memcpy(all, buf, sizeof(buf));
  memcpy(all + sizeof(buf), extra, (count - 1) * 5);
  for (uint8_t i = 0; i < count; ++i) {
    // The first CST3530 point begins at byte 4; later points follow it.
    const uint8_t* p = all + 4 + i * 5;
    cst3530_points[i].x = (uint16_t)(((p[3] & 0x0F) << 8) | p[0]);
    cst3530_points[i].y = (uint16_t)(((p[3] & 0xF0) << 4) | p[1]);
    cst3530_points[i].strength = p[2];
  }
  cst3530_point_count = count;
  cst3530_end_read();
  return true;
}

static bool cst3530_touch(uint16_t* x, uint16_t* y) {
  if (!x || !y || !cst3530_read()) return false;
  *x = cst3530_points[0].x;
  *y = cst3530_points[0].y;
  cst3530_point_count = 0;
  return true;
}

#endif
