#pragma once
#include <math.h>

#include "Effect.h"

// Kitchen/dinner timer on the ring: the whole perimeter stays lit — the
// remaining time is full-brightness amber, the elapsed part trails behind
// the bright marker at reduced intensity (so the lounge keeps its light),
// the final minute pulses the remaining arc, and when time is up the lamp
// breathes green until the mode changes. Start it from the web UI or Home
// Assistant with a duration; selecting the mode directly restarts the last
// duration.
//
// Tuning:
//   Remaining arc color — the (lastMinute ? ... : ...) triples: normal
//     amber 42000/16000/800, last-minute red 52000/6000/glow*800.
//   Elapsed trail — setPixel(i, 25000, 9500, 500): same hue at ~60%; raise
//     or lower all three together to change how bright the room stays.
//   Last-minute pulse — the (el % 1000u)/1000 line: 1000 = pulse period ms,
//     0.45f floor = how deep it dips.
//   Head marker — setPixel(lit-1, 65535, 40000, 12000).
//   Done state — the fill(v*3500, v*42000, v*6000) green and its 3000 ms
//     breathe period.
class DinnerTimer : public Effect {
public:
  const char* name() const override { return "Dinner timer"; }

  void start(uint32_t minutes) {
    if (minutes < 1) minutes = 1;
    if (minutes > 720) minutes = 720;
    durMs_ = minutes * 60000UL;
    restart_ = true;
  }

  uint32_t minutes() const { return durMs_ / 60000UL; }

  void begin(HD108Strip& strip) override {
    Effect::begin(strip);
    restart_ = true;
  }

  // update() redraws every pixel each frame — don't reset the countdown.
  void repaint(HD108Strip&) override {}

  void update(HD108Strip& strip, uint32_t now) override {
    if (restart_) {
      restart_ = false;
      t0_ = now;
    }
    uint32_t el = now - t0_;
    const uint16_t n = strip.size();

    if (el >= durMs_) {
      // Keep el bounded so the uint32 subtraction can't wrap if the done
      // state is left showing for weeks (advance t0_ by whole breathe
      // periods, preserving the phase).
      if (el >= durMs_ + 3000u) {
        t0_ += ((el - durMs_) / 3000u) * 3000u;
        el = now - t0_;
      }
      // Done: calm green breathing until the mode is changed.
      const float ph = (float)((el - durMs_) % 3000u) / 3000.0f;
      const float v = 0.25f + 0.55f * (0.5f - 0.5f * cosf(TWO_PI * ph));
      strip.fill((uint16_t)(v * 3500.0f), (uint16_t)(v * 42000.0f),
                 (uint16_t)(v * 6000.0f));
      return;
    }

    const float remain = 1.0f - (float)el / (float)durMs_;
    const bool lastMinute = durMs_ - el <= 60000u;
    float glow = 1.0f;
    if (lastMinute) {  // urgent once-per-second pulse
      const float ph = (float)(el % 1000u) / 1000.0f;
      glow = 0.45f + 0.55f * (0.5f + 0.5f * cosf(TWO_PI * ph));
    }
    const uint16_t lit = (uint16_t)(remain * n + 0.5f);
    for (uint16_t i = 0; i < n; ++i) {
      if (i < lit) {
        // Remaining time: full amber (pulsing red-ish in the last minute).
        strip.setPixel(i, (uint16_t)(glow * (lastMinute ? 52000.0f : 42000.0f)),
                       (uint16_t)(glow * (lastMinute ? 6000.0f : 16000.0f)),
                       (uint16_t)(glow * 800.0f));
      } else {
        // Elapsed time: same warm tone at ~60% — keeps the room lit, steady
        // through the last-minute pulse.
        strip.setPixel(i, 25000, 9500, 500);
      }
    }
    if (lit > 0) {
      strip.setPixel(lit - 1, 65535, 40000, 12000);  // bright head at the edge
    }
  }

private:
  uint32_t durMs_ = 15 * 60000UL;
  uint32_t t0_ = 0;
  bool restart_ = true;
};
