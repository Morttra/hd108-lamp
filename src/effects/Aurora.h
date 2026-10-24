#pragma once
#include <math.h>

#include "ColorUtil.h"
#include "Effect.h"
#include "geometry.h"

// Slow northern-lights curtains drifting around the loop: overlapping sine
// fields sweep through the active palette's hue band, with dark gaps between
// the curtains. The 16-bit range keeps the dim tails smooth.
//
// Two settings, both persisted and exposed in the web UI and Home Assistant:
//   palette — which hue band the curtains move through (kPalettes below).
//   speed   — drift tempo as a percentage of the default, 10..300.
//
// Tuning:
//   kPalettes rows — {name, hue, span, saturation}. Hue is 0..65535 once
//     around the wheel (0 red, ~21800 green, ~43700 blue, ~54600 magenta);
//     the band runs from hue to hue+span and may wrap past red. The wheel is
//     in linear light, so it reads yellower/pinker than sRGB intuition:
//     a hue of ~1500 is already red-orange, ~6000 looks yellow, and any
//     saturation below 65535 tints a red toward pink. Add a row to add a
//     palette; palettes are remembered by name.
//   0.050/0.023/0.011 — base drift of the three wave fields in cycles per
//     second (at speed 100 %).
//   u * 3.0f / 5.0f / 1.0f — how many waves of each field fit around the
//     ring; more waves = narrower curtains.
//   inten * 58000 + 900 — overall brightness and the never-black floor.
class Aurora : public Effect {
public:
  struct Palette {
    const char* name;
    uint16_t hue;   // one end of the band
    uint16_t span;  // band width from there, wrapping around the wheel
    uint16_t sat;   // 0..65535; lower = paler, icier colors
  };

  static constexpr Palette kPalettes[] = {
      {"Northern", 21800, 24000, 62000},  // green -> violet (the classic)
      {"Rose", 47000, 13000, 58000},      // violet -> magenta -> pink
      {"Dawn", 56000, 16000, 62000},      // magenta -> red -> orange
      {"Ember", 0, 1500, 65535},          // deep red, barely warming
  };
  static constexpr uint8_t kPaletteCount =
      sizeof(kPalettes) / sizeof(kPalettes[0]);
  static constexpr uint16_t kMinSpeed = 10;
  static constexpr uint16_t kMaxSpeed = 300;

  const char* name() const override { return "Aurora"; }

  static const char* paletteName(uint8_t i) {
    return kPalettes[i < kPaletteCount ? i : 0].name;
  }
  static uint8_t paletteByName(const char* name) {
    for (uint8_t i = 0; i < kPaletteCount; ++i) {
      if (strcmp(kPalettes[i].name, name) == 0) return i;
    }
    return 0;
  }

  void setPalette(uint8_t i) { palette_ = i < kPaletteCount ? i : 0; }
  uint8_t palette() const { return palette_; }

  // Drift tempo in percent of the default; clamped to kMinSpeed..kMaxSpeed.
  void setSpeed(uint16_t percent) {
    speed_ = percent < kMinSpeed   ? kMinSpeed
             : percent > kMaxSpeed ? kMaxSpeed
                                   : percent;
  }
  uint16_t speed() const { return speed_; }

  void begin(HD108& strip) override {
    Effect::begin(strip);
    haveLast_ = false;
  }

  // update() redraws every pixel each frame.
  void repaint(HD108&) override {}

  void update(HD108& strip, uint32_t now) override {
    // Phase accumulators instead of an absolute clock: now*0.001f in float
    // quantizes with uptime until the drift turns into visible steps.
    if (!haveLast_) {
      haveLast_ = true;
      lastMs_ = now;
    }
    const float dt = (float)(uint32_t)(now - lastMs_) * 0.001f *
                     ((float)speed_ * 0.01f);
    lastMs_ = now;
    p1_ += 0.050f * dt;  p1_ -= floorf(p1_);
    p2_ -= 0.023f * dt;  p2_ -= floorf(p2_);
    p3_ += 0.011f * dt;  p3_ -= floorf(p3_);

    const Palette& pal = kPalettes[palette_];
    const uint16_t n = strip.size();
    for (uint16_t i = 0; i < n; ++i) {
      const float u = geo::frac(i);
      const float w1 = sinf(TWO_PI * (u * 3.0f + p1_));
      const float w2 = sinf(TWO_PI * (u * 5.0f + p2_) + 1.7f);
      const float w3 = sinf(TWO_PI * (u * 1.0f + p3_) + 4.1f);
      const float m = (w1 + w2 + w3) * (1.0f / 3.0f);  // -1..1
      const float inten = m > 0.0f ? m * m : 0.0f;
      const float huef = 0.5f + 0.5f * w3;  // drift across the band
      const uint16_t hue =
          (uint16_t)((uint32_t)pal.hue + (uint32_t)(huef * (float)pal.span));
      const uint16_t v = (uint16_t)(inten * 58000.0f) + 900;
      uint16_t r, g, b;
      hsv2rgb16(hue, pal.sat, v, r, g, b);
      strip.setPixel(i, r, g, b);
    }
  }

private:
  uint8_t palette_ = 0;
  uint16_t speed_ = 100;
  float p1_ = 0.0f, p2_ = 0.0f, p3_ = 0.0f;
  uint32_t lastMs_ = 0;
  bool haveLast_ = false;
};
