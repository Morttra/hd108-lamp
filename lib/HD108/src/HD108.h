#pragma once
#include <Arduino.h>
#include <SPI.h>

// Driver for HD108 16-bit RGB LEDs (APA102-style two-wire bus: data + clock,
// driven here with ESP32 hardware SPI). The class is HD108Strip: the bare
// name HD108 is FastLED's chipset tag since 3.10, and the two must coexist.
//
// Wire format (per the HD108 datasheet):
//   start frame : 128 zero bits
//   pixel frame : 64 bits, MSB first =
//                 [1][5-bit gain][5-bit gain][5-bit gain][16b c0][16b c1][16b c2]
//                 The three 5-bit fields set the constant-current gain (n/31
//                 of 17 mA) per output; the 16-bit fields are PWM gray levels.
//   end frame   : >= 64 one bits, extended by one extra clock edge per two
//                 LEDs so the last pixels in the cascade get clocked through.
//
// The datasheet lists the color fields in B,G,R order; some production
// batches are R,G,B. The order is configurable — verify with a pure red
// fill and set LED_COLOR_ORDER in config.h accordingly.
//
// This class owns a preassembled wire buffer: setPixel() writes straight into
// it and show() is a single blocking SPI transfer (~2.2 ms for 340 LEDs at
// 10 MHz).

enum class ColorOrder : uint8_t { BGR, RGB, GRB, BRG, RBG, GBR };

class HD108Strip {
public:
  // `spi` selects the hardware bus; the default is the global VSPI object.
  // For a second strip on the ESP32, pass an SPIClass(HSPI) instance.
  HD108Strip(uint16_t numLeds, int8_t dataPin, int8_t clockPin,
        uint32_t spiHz = 10000000UL, SPIClass& spi = SPI);
  ~HD108Strip();

  // Allocates the wire buffer and claims the SPI bus. Returns false if the
  // buffer allocation fails.
  bool begin();
  void end();

  void setColorOrder(ColorOrder order);

  // Rotates the ring: logical index 0 maps to physical LED `offset` (may be
  // negative). Lets effects assume position 0 is exactly on a landmark
  // (e.g. the lamp's first corner) regardless of where the strip was cut.
  void setOffset(int16_t offset) { offset_ = offset; }

  // 5-bit constant-current gain (0..31) applied to every pixel. This is true
  // current dimming — it scales brightness without losing PWM resolution, so
  // it works well as a flicker-free master brightness.
  void setGlobalGain(uint8_t gain);
  uint8_t globalGain() const { return gain_; }
  // Per-pixel override of the current gain (0..31) — the HD108 frame carries
  // one per LED. setGlobalGain() resets every pixel to the global value.
  void setPixelGain(uint16_t i, uint8_t gain);

  // Linear 16-bit channel values.
  void setPixel(uint16_t i, uint16_t r, uint16_t g, uint16_t b);
  // 8-bit sRGB-ish values, gamma-expanded (2.2) to 16 bits.
  void setPixel8(uint16_t i, uint8_t r, uint8_t g, uint8_t b);
  void getPixel(uint16_t i, uint16_t& r, uint16_t& g, uint16_t& b) const;

  void fill(uint16_t r, uint16_t g, uint16_t b);
  void clear() { fill(0, 0, 0); }

  // Pushes the buffer to the strip.
  void show();

  uint16_t size() const { return n_; }

  // 8-bit -> 16-bit gamma (2.2) expansion.
  static uint16_t gamma16(uint8_t v);

private:
  void writeHeader(uint16_t phys, uint8_t gain);

  // logical -> physical index with ring rotation
  uint16_t phys(uint16_t i) const {
    const int32_t p = ((int32_t)i + offset_) % (int32_t)n_;
    return (uint16_t)(p < 0 ? p + n_ : p);
  }

  uint16_t n_;
  int8_t dataPin_;
  int8_t clockPin_;
  uint32_t hz_;
  SPIClass* spi_;

  uint8_t* buf_ = nullptr;
  size_t len_ = 0;
  size_t endLen_ = 0;

  uint8_t gain_ = 31;
  uint8_t slotR_ = 2, slotG_ = 1, slotB_ = 0;  // channel slots within a frame
  int16_t offset_ = 0;
  bool begun_ = false;
};
