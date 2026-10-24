#pragma once
#include <math.h>

#include "Effect.h"

// Pomodoro: 25 min work / 5 min break cycles, with a 15 min long break
// after every 4th session. Work is a warm amber countdown arc (like the
// dinner timer), breaks are green. Each phase change opens with a few
// seconds of full-ring pulsing in the new color — deliberately unmissable,
// because forgetting breaks is the whole problem. Small white pips near
// LED 0 count the completed sessions in the current set of four.
// Selecting the mode starts a fresh work session.
//
// Tuning:
//   kWorkMin / kBreakMin / kLongBreakMin / kSessionsPerLong — the schedule.
//   kFlourishMs — how long the phase-change pulse lasts (0 disables it).
//   Work colors — daylight arc (60000,37000,20000), trail (36000,22000,12000).
//   Break colors — arc (3500,42000,6000), trail (1800,21000,3000).
//   Pips — kPipLen LEDs per completed session, white, near LED 0.
class Pomodoro : public Effect {
public:
  const char* name() const override { return "Pomodoro"; }

  void begin(HD108Strip& strip) override {
    Effect::begin(strip);
    working_ = true;
    sessions_ = 0;
    phaseDurMs_ = kWorkMin * 60000UL;
    restart_ = true;
  }

  // update() redraws every pixel each frame.
  void repaint(HD108Strip&) override {}

  void update(HD108Strip& strip, uint32_t now) override {
    if (restart_) {
      restart_ = false;
      phaseStart_ = now;
    }
    uint32_t el = now - phaseStart_;

    // Phase finished -> advance.
    if (el >= phaseDurMs_) {
      if (working_) {
        ++sessions_;
        working_ = false;
        phaseDurMs_ = (sessions_ % kSessionsPerLong == 0)
                          ? kLongBreakMin * 60000UL
                          : kBreakMin * 60000UL;
      } else {
        working_ = true;
        phaseDurMs_ = kWorkMin * 60000UL;
      }
      phaseStart_ = now;
      el = 0;
    }

    const uint16_t n = strip.size();

    // Opening flourish: pulse the whole ring in the new phase's color.
    if (el < kFlourishMs) {
      const float ph = (float)(el % 1000u) / 1000.0f;
      const float v = 0.35f + 0.65f * (0.5f - 0.5f * cosf(TWO_PI * ph));
      if (working_) {
        strip.fill((uint16_t)(v * 62000.0f), (uint16_t)(v * 38000.0f),
                   (uint16_t)(v * 21000.0f));
      } else {
        strip.fill((uint16_t)(v * 4000.0f), (uint16_t)(v * 52000.0f),
                   (uint16_t)(v * 7000.0f));
      }
      return;
    }

    // Countdown arc: remaining phase time at full color, elapsed trail dim.
    const float remain = 1.0f - (float)el / (float)phaseDurMs_;
    const uint16_t lit = (uint16_t)(remain * n + 0.5f);
    for (uint16_t i = 0; i < n; ++i) {
      if (i < lit) {
        if (working_) strip.setPixel(i, 60000, 37000, 20000);  // daylight
        else strip.setPixel(i, 3500, 42000, 6000);
      } else {
        if (working_) strip.setPixel(i, 36000, 22000, 12000);
        else strip.setPixel(i, 1800, 21000, 3000);
      }
    }
    if (lit > 0) {
      strip.setPixel(lit - 1, 65535, 40000, 12000);  // head marker
    }

    // Session pips: one short white marker per completed session (set of 4).
    const uint8_t pips = sessions_ % kSessionsPerLong;
    for (uint8_t p = 0; p < pips; ++p) {
      for (uint8_t j = 0; j < kPipLen; ++j) {
        const uint16_t idx = 2 + p * (kPipLen + 2) + j;
        if (idx < n) strip.setPixel(idx, 50000, 50000, 50000);
      }
    }
  }

private:
  static const uint32_t kWorkMin = 25;
  static const uint32_t kBreakMin = 5;
  static const uint32_t kLongBreakMin = 15;
  static const uint8_t kSessionsPerLong = 4;
  static const uint32_t kFlourishMs = 5000;
  static const uint8_t kPipLen = 2;

  bool working_ = true;
  uint8_t sessions_ = 0;
  uint32_t phaseDurMs_ = kWorkMin * 60000UL;
  uint32_t phaseStart_ = 0;
  bool restart_ = true;
};
