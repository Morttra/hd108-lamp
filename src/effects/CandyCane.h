#pragma once
#include <math.h>

#include "Effect.h"
#include "geometry.h"

// Candy cane: red and white bands rotating slowly around the perimeter, with
// soft edges so the motion glides rather than stepping LED by LED. For the
// Christmas table.
//
// Tuning:
//   kBands — bands around the ring; keep it even so the seam tiles
//     (20 on 340 px = 17 LEDs, ~35 cm, per band).
//   kLapS — seconds for the pattern to travel once around the ring.
//   kEdge — half-width of the soft edge between bands, in band units
//     (0.1 of a 17-LED band ≈ 1.7 LEDs each side of the boundary).
//   kRed / kWhite — the two colors in 8-bit sRGB terms; gamma is applied.
class CandyCane : public Effect {
public:
  const char* name() const override { return "Candy cane"; }

  void begin(HD108Strip& strip) override {
    Effect::begin(strip);
    haveLast_ = false;
    toLinear(kRed, red_);
    toLinear(kWhite, white_);
  }

  // update() redraws every pixel each frame.
  void repaint(HD108Strip&) override {}

  void update(HD108Strip& strip, uint32_t now) override {
    if (!haveLast_) {
      haveLast_ = true;
      lastMs_ = now;
    }
    const float dt = (float)(uint32_t)(now - lastMs_) * 0.001f;
    lastMs_ = now;
    phase_ += dt / kLapS;  // in laps
    phase_ -= floorf(phase_);

    const uint16_t n = strip.size();
    for (uint16_t i = 0; i < n; ++i) {
      // Position in band units, moving with the phase; x in [0, 2) selects
      // red [0,1) or white [1,2).
      float s = (geo::frac(i) - phase_) * (float)kBands;
      s -= floorf(s * 0.5f) * 2.0f;  // wrap into [0, 2)
      const bool white = s >= 1.0f;
      // Distance to the nearest band boundary, for the soft edge.
      const float t = s - floorf(s);
      const float d = t < 0.5f ? t : 1.0f - t;
      const float mix = d < kEdge ? d / kEdge : 1.0f;  // 0 at boundary
      const float w = white ? 0.5f + 0.5f * mix : 0.5f - 0.5f * mix;
      strip.setPixel(i, lerp(red_[0], white_[0], w), lerp(red_[1], white_[1], w),
                     lerp(red_[2], white_[2], w));
    }
  }

private:
  static constexpr uint8_t kBands = 20;
  static constexpr float kLapS = 60.0f;
  static constexpr float kEdge = 0.1f;
  static constexpr uint8_t kRed[3] = {255, 0, 0};
  static constexpr uint8_t kWhite[3] = {255, 235, 205};  // slightly warm

  static void toLinear(const uint8_t c[3], uint16_t out[3]) {
    for (int k = 0; k < 3; ++k) out[k] = HD108Strip::gamma16(c[k]);
  }
  static uint16_t lerp(uint16_t a, uint16_t b, float w) {
    return (uint16_t)((float)a + ((float)b - (float)a) * w);
  }

  uint16_t red_[3] = {0, 0, 0};
  uint16_t white_[3] = {0, 0, 0};
  float phase_ = 0.0f;
  uint32_t lastMs_ = 0;
  bool haveLast_ = false;
};
