#pragma once
#include "Effect.h"

// Cozy static warm white (~2700 K feel from RGB). Use the global gain
// (brightness slider in the web UI) to dim — it is current-based, so it
// stays flicker-free and keeps full 16-bit resolution.
//
// Tuning: the setPixel8(255, 147, 41) triple is the color in familiar
// 8-bit sRGB terms (gamma is applied for you). Warmer: lower the 41.
// Cooler/neutral: try (255, 197, 143) ≈ 4000 K.
class WarmWhite : public Effect {
public:
  const char* name() const override { return "Warm white"; }

  void begin(HD108Strip& strip) override {
    for (uint16_t i = 0; i < strip.size(); ++i) {
      strip.setPixel8(i, 255, 147, 41);
    }
  }

  void update(HD108Strip&, uint32_t) override {}
};
