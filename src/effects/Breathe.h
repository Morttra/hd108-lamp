#pragma once
#include <math.h>

#include "Effect.h"

// Guided 4-7-8 breathing (Weil technique): 4 s inhale — the lamp brightens;
// 7 s hold — steady at full; 8 s exhale — long dim down to a faint ember.
// One cycle is 19 s (~3 breaths/min).
//
// The hold cue: a small fireball — a hot orange-red head with a short tail,
// a few pixels — makes exactly one lap over the fully lit ring during the
// 7 s hold. Where it is around the rectangle shows how much hold is left;
// its return to the corner is the "exhale now" cue. The other transitions
// carry themselves: light rising = inhale, light falling = exhale.
//
// Tuning:
//   kInhaleS / kHoldS / kExhaleS — phase lengths (4/7/8; use 4/4/4 with
//     kRestS 4 for box breathing).
//   kRestS — optional pause at the bottom (0 = none).
//   kFloor — how dim the exhale bottoms out (never fully dark).
//   kTailLeds — fireball tail length; the head sharpness is the 0.8f in
//     the leading-edge gaussian.
//   The blend factors (0.75/0.9 and the red push) — how hot the fireball
//     reads against the warm-white base.
class Breathe : public Effect {
public:
  const char* name() const override { return "Breathe 4-7-8"; }

  void begin(HD108& strip) override {
    Effect::begin(strip);
    haveLast_ = false;
    t_ = 0.0f;
  }

  // update() redraws every pixel each frame.
  void repaint(HD108&) override {}

  void update(HD108& strip, uint32_t now) override {
    if (!haveLast_) {
      haveLast_ = true;
      lastMs_ = now;
    }
    const uint32_t dtMs = now - lastMs_;
    lastMs_ = now;
    t_ += (float)(dtMs > 100 ? 100 : dtMs) * 0.001f;
    const float cycle = kInhaleS + kHoldS + kExhaleS + kRestS;
    while (t_ >= cycle) t_ -= cycle;

    float v;         // breath level 0..1
    float holdP = -1.0f;  // hold progress 0..1, <0 outside the hold
    if (t_ < kInhaleS) {
      v = smooth(t_ / kInhaleS);
    } else if (t_ < kInhaleS + kHoldS) {
      v = 1.0f;
      holdP = (t_ - kInhaleS) / kHoldS;
    } else if (t_ < kInhaleS + kHoldS + kExhaleS) {
      v = smooth(1.0f - (t_ - kInhaleS - kHoldS) / kExhaleS);
    } else {
      v = 0.0f;
    }

    const uint16_t n = strip.size();
    const float l = kFloor + (1.0f - kFloor) * v;
    const uint16_t br = (uint16_t)(l * 62000.0f);
    const uint16_t bg = (uint16_t)(l * l * 27000.0f);
    const uint16_t bb = (uint16_t)(l * l * l * 3200.0f);
    strip.fill(br, bg, bb);

    if (holdP < 0.0f) return;

    // Fireball: one lap during the hold, sub-pixel smooth. Sharp leading
    // edge, short glowing tail behind the direction of travel.
    const float head = holdP * (float)n;
    const int span = (int)kTailLeds + 3;
    for (int o = -2; o <= span; ++o) {  // o>0 is behind the head
      const int idx = (int)floorf(head) - o;
      const uint16_t i = (uint16_t)((idx % (int)n + (int)n) % (int)n);
      const float d = head - (float)(int)floorf(head) + (float)o;
      const float w = d < 0.0f ? expf(-(d * d) / 0.8f)
                               : expf(-d / (kTailLeds * 0.45f));
      if (w < 0.02f) continue;
      const uint32_t r = br + (uint32_t)(w * (float)(65535 - br));
      strip.setPixel(i, r > 65535 ? 65535 : (uint16_t)r,
                     (uint16_t)((float)bg * (1.0f - 0.75f * w)),
                     (uint16_t)((float)bb * (1.0f - 0.9f * w)));
    }
  }

private:
  static constexpr float kInhaleS = 4.0f;
  static constexpr float kHoldS = 7.0f;
  static constexpr float kExhaleS = 8.0f;
  static constexpr float kRestS = 0.0f;
  static constexpr float kFloor = 0.035f;
  static constexpr float kTailLeds = 7.0f;

  // Smoothstep: gentle turnarounds, near-linear in the middle — easy to
  // pace lungs against.
  static float smooth(float x) { return x * x * (3.0f - 2.0f * x); }

  float t_ = 0.0f;
  uint32_t lastMs_ = 0;
  bool haveLast_ = false;
};
