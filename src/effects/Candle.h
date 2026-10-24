#pragma once
#include "Effect.h"

// Candlelight: a ring of independent virtual flames. Each flame does its own
// smoothed random walk (like a real candle disturbed by air), and every LED
// blends between its two nearest flames — so pools of warm flicker drift
// around the lamp instead of the whole ring pulsing in unison.
//
// Tuning:
//   kFlames — number of independent flames; fewer = larger, lazier pools.
//   0.35f + random(0, 651)/1000 — flicker depth: min 0.35, max 1.0.
//   random(240, 1040) — ms between a flame picking a new target; shorter =
//     more nervous flame.
//   0.035f — smoothing toward the target; lower = softer, dreamier.
//   v*62000 / v²*24000 / v³*2200 — the warm color curve: green and blue
//     collapse faster than red as a flame dims (higher powers = warmer
//     dips). Raise 24000 for a yellower, 2200 for a whiter flame.
class Candle : public Effect {
public:
  const char* name() const override { return "Candle"; }

  void begin(HD108& strip) override {
    Effect::begin(strip);
    for (int j = 0; j < kFlames; ++j) {
      level_[j] = 0.6f;
      target_[j] = 0.6f;
      nextPick_[j] = millis();  // pick a target on the first frame
    }
  }

  // update() redraws every pixel each frame.
  void repaint(HD108&) override {}

  void update(HD108& strip, uint32_t now) override {
    for (int j = 0; j < kFlames; ++j) {
      if ((int32_t)(now - nextPick_[j]) >= 0) {  // wrap-safe deadline
        target_[j] = 0.35f + (float)random(0, 651) / 1000.0f;  // 0.35 .. 1.0
        nextPick_[j] = now + random(240, 1040);
      }
      level_[j] += (target_[j] - level_[j]) * 0.035f;
    }

    const uint16_t n = strip.size();
    for (uint16_t i = 0; i < n; ++i) {
      const float f = (float)i * kFlames / n;
      const int j = (int)f;
      const float t = f - (float)j;
      const float v = level_[j % kFlames] * (1.0f - t) +
                      level_[(j + 1) % kFlames] * t;
      strip.setPixel(i, (uint16_t)(v * 62000.0f),
                     (uint16_t)(v * v * 24000.0f),
                     (uint16_t)(v * v * v * 2200.0f));
    }
  }

private:
  static const int kFlames = 24;  // flame spacing ~14 LEDs / ~27 cm
  float level_[kFlames];
  float target_[kFlames];
  uint32_t nextPick_[kFlames];
};
