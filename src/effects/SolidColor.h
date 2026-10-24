#pragma once
#include "Effect.h"

// A single settable color for the lamp. Home Assistant's color picker
// drives this mode; it is also handy as a plain preset from the web UI.
class SolidColor : public Effect {
public:
  const char* name() const override { return "Solid color"; }

  void set(uint8_t r, uint8_t g, uint8_t b) {
    r_ = r;
    g_ = g;
    b_ = b;
    dirty_ = true;
  }

  uint8_t red() const { return r_; }
  uint8_t green() const { return g_; }
  uint8_t blue() const { return b_; }

  void begin(HD108Strip& strip) override { apply(strip); }

  void update(HD108Strip& strip, uint32_t) override {
    if (dirty_) apply(strip);
  }

private:
  void apply(HD108Strip& strip) {
    dirty_ = false;
    for (uint16_t i = 0; i < strip.size(); ++i) {
      strip.setPixel8(i, r_, g_, b_);
    }
  }

  uint8_t r_ = 255, g_ = 180, b_ = 100;
  bool dirty_ = true;
};
