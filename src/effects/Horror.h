#pragma once
#include <math.h>

#include "Effect.h"

// Horror-movie corridor. The ring is a row of fluorescent tubes over a dim,
// slowly pulsing background. Every second or so a tube malfunctions — a
// stutter of rapid flashes that may catch and hold a wavering second before
// dying. Both colors are the user's: the flash color and the background
// color, picked in the web UI and remembered across reboots.
//
// Tuning:
//   kChance — malfunction odds (per 10000) per tube decision; ~220 gives
//     one or two bursts a second around the ring.
//   kTubes — tube count around the ring (~21 LEDs per tube at 16); the last
//     10 % of each tube stays dark as the fixture end.
//   kBgLevel / kBgPulse / kBgPeriodS — background intensity as a fraction
//     of the picked color, how deep the pulse dips, and its period.
//   The random() draws in stepTube() — burst/hold lengths and stutter
//     timing; widen the random(15, 90) "on" window for lazier flashes.
class Horror : public Effect {
public:
  Horror() {
    setStrobe(0xD1F5E5);      // sickly cold white
    setBackground(0xFF190F);  // deep red
  }

  const char* name() const override { return "Horror"; }

  uint16_t frameMs() const override { return 8; }

  // Colors as 0xRRGGBB in familiar 8-bit sRGB terms (gamma applied here).
  void setStrobe(uint32_t rgb) {
    strobe_ = rgb & 0xFFFFFF;
    toLinear(strobe_, strobeLin_);
  }
  uint32_t strobe() const { return strobe_; }

  void setBackground(uint32_t rgb) {
    bg_ = rgb & 0xFFFFFF;
    toLinear(bg_, bgLin_);
  }
  uint32_t background() const { return bg_; }

  void begin(HD108Strip& strip) override {
    Effect::begin(strip);
    reset_ = true;
  }

  // update() redraws every pixel each frame.
  void repaint(HD108Strip&) override {}

  void update(HD108Strip& strip, uint32_t now) override {
    if (reset_) {
      reset_ = false;
      for (Tube& t : tubes_) {
        t = Tube{};
        t.next = now;
      }
      pulsePhase_ = 0.0f;
      lastMs_ = now;
    }

    // Background: slow pulse between kBgLevel*(1-kBgPulse) and kBgLevel.
    const float dt = (float)(uint32_t)(now - lastMs_) * 0.001f;
    lastMs_ = now;
    pulsePhase_ += dt / kBgPeriodS;
    pulsePhase_ -= floorf(pulsePhase_);
    const float pulse =
        kBgLevel * (1.0f - kBgPulse * (0.5f + 0.5f * cosf(TWO_PI * pulsePhase_)));

    for (Tube& t : tubes_) stepTube(t, now);

    const uint16_t n = strip.size();
    for (uint16_t i = 0; i < n; ++i) {
      const float ft = (float)i * kTubes / n;
      uint8_t k = (uint8_t)ft;
      if (k >= kTubes) k = kTubes - 1;
      const float along = ft - (float)k;  // 0..1 along this tube
      const float lvl = along < 0.9f ? tubes_[k].level : 0.0f;  // dark end cap
      uint32_t rgb[3];
      for (int c = 0; c < 3; ++c) {
        rgb[c] = (uint32_t)(bgLin_[c] * pulse + strobeLin_[c] * lvl);
        if (rgb[c] > 65535) rgb[c] = 65535;
      }
      strip.setPixel(i, (uint16_t)rgb[0], (uint16_t)rgb[1], (uint16_t)rgb[2]);
    }
  }

private:
  struct Tube {
    uint8_t state = 0;  // 0 dark, 1 stuttering burst, 2 caught and holding
    bool lit = false;
    float target = 0.0f;  // intensity while lit, 0..1
    float level = 0.0f;   // smoothed output
    uint32_t next = 0;    // next decision time
    uint32_t stateEnd = 0;
  };

  static constexpr uint8_t kTubes = 16;
  static constexpr uint16_t kChance = 220;  // per 10000, per decision
  static constexpr float kBgLevel = 0.15f;
  static constexpr float kBgPulse = 0.6f;  // dips to 40 % of kBgLevel
  static constexpr float kBgPeriodS = 3.5f;

  static void toLinear(uint32_t rgb, float out[3]) {
    out[0] = HD108Strip::gamma16((rgb >> 16) & 0xFF);
    out[1] = HD108Strip::gamma16((rgb >> 8) & 0xFF);
    out[2] = HD108Strip::gamma16(rgb & 0xFF);
  }

  static void stepTube(Tube& t, uint32_t now) {
    if ((int32_t)(now - t.next) >= 0) {
      switch (t.state) {
        case 0:  // dark: roll for a malfunction
          if (random(10000) < kChance) {
            t.state = 1;
            t.stateEnd = now + random(250, 1400);
            t.lit = true;
            t.target = 0.3f + random(0, 701) / 1000.0f;
            t.next = now + random(15, 80);
          } else {
            t.next = now + random(100, 400);
          }
          break;
        case 1:  // stuttering burst
          if ((int32_t)(now - t.stateEnd) >= 0) {
            if (random(100) < 35) {  // the tube catches
              t.state = 2;
              t.stateEnd = now + random(500, 2500);
              t.lit = true;
              t.target = 0.5f + random(0, 401) / 1000.0f;
              t.next = now + 40;
            } else {
              t.state = 0;
              t.lit = false;
              t.next = now + random(100, 400);
            }
          } else {
            t.lit = !t.lit;
            if (t.lit) t.target = 0.25f + random(0, 751) / 1000.0f;
            t.next = now + (t.lit ? random(15, 90) : random(25, 220));
          }
          break;
        default:  // holding, wavering, the odd dropout
          if ((int32_t)(now - t.stateEnd) >= 0) {
            t.state = 0;
            t.lit = false;
            t.next = now + random(100, 400);
          } else {
            t.lit = random(100) >= 8;
            t.target = 0.5f + random(0, 401) / 1000.0f;
            t.next = now + random(30, 120);
          }
          break;
      }
    }
    const float goal = t.lit ? t.target : 0.0f;
    t.level += (goal - t.level) * (goal > t.level ? 0.85f : 0.5f);
  }

  Tube tubes_[kTubes];
  uint32_t strobe_ = 0;
  uint32_t bg_ = 0;
  float strobeLin_[3] = {0, 0, 0};
  float bgLin_[3] = {0, 0, 0};
  float pulsePhase_ = 0.0f;
  uint32_t lastMs_ = 0;
  bool reset_ = true;
};
