#pragma once
#include <math.h>

#include "Effect.h"

// Speed showcase: two comets sprint around the ring in opposite directions,
// sweeping between a lazy cruise and ~8 laps per second, with motion-blur
// tails that stretch with speed and a bright flash where they cross. Runs
// the render loop at ~166 fps (frameMs 6) — at normal frame rates this
// would tear into dashes; on the HD108 pipeline it stays liquid.
//
// Tuning:
//   0.3f + s * 7.7f — cruise and top speed in laps/second.
//   envPhase_ += dt / 20.0f — the 20 s cruise<->sprint sweep period.
//   tail = 0.02f + 0.06f * s — tail length (fraction of the ring) at rest
//     and how much it stretches at speed.
//   The r/g/b lines — comet colors: in1 terms = ice-blue comet, in2 terms =
//     amber comet, the trailing +250/+90/+20 = faint background glow.
class Race : public Effect {
public:
  const char* name() const override { return "Race"; }

  uint16_t frameMs() const override { return 6; }

  void begin(HD108& strip) override {
    Effect::begin(strip);
    haveLast_ = false;
    pos1_ = 0.1f;
    pos2_ = 0.8f;
    envPhase_ = 0.0f;
  }

  // update() redraws every pixel each frame.
  void repaint(HD108&) override {}

  void update(HD108& strip, uint32_t now) override {
    if (!haveLast_) {
      haveLast_ = true;
      lastMs_ = now;
    }
    uint32_t dtMs = now - lastMs_;
    lastMs_ = now;
    if (dtMs > 50) dtMs = 50;  // clamp across hiccups/mode switches
    const float dt = (float)dtMs * 0.001f;

    // Speed envelope: 20 s sweep, cruise <-> sprint.
    envPhase_ += dt / 20.0f;
    envPhase_ -= floorf(envPhase_);
    const float s = 0.5f - 0.5f * cosf(TWO_PI * envPhase_);  // 0..1
    const float lapsPerSec = 0.3f + s * 7.7f;

    pos1_ += lapsPerSec * dt;
    pos1_ -= floorf(pos1_);
    pos2_ -= lapsPerSec * dt;
    pos2_ -= floorf(pos2_);
    if (pos2_ < 0.0f) pos2_ += 1.0f;

    // Tails stretch with speed, like motion blur.
    const float tail = 0.02f + 0.06f * s;

    const uint16_t n = strip.size();
    for (uint16_t i = 0; i < n; ++i) {
      const float u = (float)i / (float)n;

      float d1 = pos1_ - u;  // distance behind comet 1 (travels +u)
      d1 -= floorf(d1);
      const float in1 = expf(-d1 / tail);

      float d2 = u - pos2_;  // distance behind comet 2 (travels -u)
      d2 -= floorf(d2);
      const float in2 = expf(-d2 / tail);

      // Comet 1 ice-blue, comet 2 amber; crossings sum toward white.
      uint32_t r = (uint32_t)(in1 * 3000.0f + in2 * 60000.0f) + 250;
      uint32_t g = (uint32_t)(in1 * 30000.0f + in2 * 22000.0f) + 90;
      uint32_t b = (uint32_t)(in1 * 62000.0f + in2 * 1500.0f) + 20;
      strip.setPixel(i, r > 65535 ? 65535 : (uint16_t)r,
                     g > 65535 ? 65535 : (uint16_t)g,
                     b > 65535 ? 65535 : (uint16_t)b);
    }
  }

private:
  float pos1_ = 0.0f, pos2_ = 0.5f;  // positions, 0..1 around the ring
  float envPhase_ = 0.0f;
  uint32_t lastMs_ = 0;
  bool haveLast_ = false;
};
