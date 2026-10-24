#pragma once
#include <math.h>

#include "ColorUtil.h"
#include "Effect.h"

// Three comets with fading tails chasing each other around the loop, hues
// slowly rotating.
//
// Tuning:
//   k — number of comets (evenly spaced).
//   lapMs — ms per lap (7000 = ~7 s per round).
//   tail = n / 7.0f — tail length in LEDs; larger divisor = shorter tail.
//   now * 2 — hue drift speed; 0 freezes the colors.
//   55000 / 62000 in hsv2rgb16 — saturation / brightness of the heads.
class Comets : public Effect {
public:
  const char* name() const override { return "Comets"; }

  void update(HD108& strip, uint32_t now) override {
    const uint16_t n = strip.size();
    const float lapMs = 7000.0f;
    const float tail = n / 7.0f;
    const int k = 3;
    // Integer modulo before the float conversion: (float)now alone loses
    // millisecond precision after ~1.5 days of uptime and the motion stutters.
    const float head = (float)(now % 7000u) / lapMs * n;

    for (uint16_t i = 0; i < n; ++i) {
      uint32_t r = 0, g = 0, b = 0;
      for (int c = 0; c < k; ++c) {
        const float h = head + (float)n * c / k;
        float d = fmodf(h - (float)i, (float)n);  // distance behind a head
        if (d < 0) d += n;
        const float in = expf(-d / tail);
        if (in < 0.004f) continue;
        uint16_t cr, cg, cb;
        const uint16_t hue = (uint16_t)((uint32_t)c * 65536 / k + now * 2);
        hsv2rgb16(hue, 55000, (uint16_t)(in * 62000.0f), cr, cg, cb);
        r += cr;
        g += cg;
        b += cb;
      }
      strip.setPixel(i, r > 65535 ? 65535 : (uint16_t)r,
                     g > 65535 ? 65535 : (uint16_t)g,
                     b > 65535 ? 65535 : (uint16_t)b);
    }
  }
};
