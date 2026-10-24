#pragma once
#include "ColorUtil.h"
#include "Effect.h"
#include "geometry.h"

// Full hue wheel wrapped once around the perimeter, slowly rotating.
//
// Tuning:
//   kPeriodMs — ms for one full revolution of the wheel.
//   (uint32_t)i * 65536 / n — one rainbow around the ring; multiply by 2
//     for a double rainbow.
//   65535 / 42000 in hsv2rgb16 — saturation / brightness.
class RainbowCycle : public Effect {
public:
  const char* name() const override { return "Rainbow"; }

  void update(HD108Strip& strip, uint32_t now) override {
    const uint32_t kPeriodMs = 24000;  // one revolution
    const uint16_t h0 =
        (uint16_t)((uint64_t)(now % kPeriodMs) * 65536 / kPeriodMs);
    const uint16_t n = strip.size();
    for (uint16_t i = 0; i < n; ++i) {
      const uint16_t h = h0 + (uint16_t)((uint32_t)i * 65536 / n);
      uint16_t r, g, b;
      hsv2rgb16(h, 65535, 42000, r, g, b);
      strip.setPixel(i, r, g, b);
    }
  }
};
