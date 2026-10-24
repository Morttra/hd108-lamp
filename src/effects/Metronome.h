#pragma once
#include <math.h>

#include "Effect.h"
#include "geometry.h"

// Visual 4/4 metronome for practice. A playhead sweeps the perimeter once
// per bar with constant time per beat; the beat boundaries are the lamp's
// four physical corners (via the geometry side counts), and crossing one
// flashes the ring — a daylight accent on beat 1, amber on beats 2-4.
//
// BPM (30..240) is set from the web UI or the Home Assistant "Metronome BPM"
// number.
//
// Tuning:
//   Accent/beat flash colors + strengths — in the beat-change branch below.
//   0.06f — flash decay time-constant in seconds (smaller = snappier click).
//   Marker and playhead colors/sizes — in the pixel loop.
class Metronome : public Effect {
public:
  const char* name() const override { return "Metronome"; }

  uint16_t frameMs() const override { return 6; }

  void setBpm(uint16_t bpm) {
    if (bpm < 30) bpm = 30;
    if (bpm > 240) bpm = 240;
    bpm_ = bpm;
  }
  uint16_t bpm() const { return bpm_; }

  void begin(HD108& strip) override {
    Effect::begin(strip);
    haveLast_ = false;
    barPhase_ = 0.0f;
    lastBeat_ = -1;
    flash_ = 0.0f;
    // Beats land on the physical corners.
    uint16_t acc = 0;
    for (uint8_t b = 0; b < kBeats; ++b) {
      bounds_[b] = acc;
      acc += geo::kSideCount[b];
    }
    bounds_[kBeats] = acc;
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
    if (dtMs > 100) dtMs = 100;
    const float dt = (float)dtMs * 0.001f;

    // bars per second = bpm / 60 / beats
    barPhase_ += dt * (float)bpm_ / (60.0f * (float)kBeats);
    barPhase_ -= floorf(barPhase_);

    const float beatF = barPhase_ * (float)kBeats;
    int beat = (int)beatF;
    if (beat >= kBeats) beat = kBeats - 1;
    if (beat != lastBeat_) {
      lastBeat_ = beat;
      if (beat == 0) {  // the accent
        flash_ = 1.0f;
        flashR_ = 62000; flashG_ = 40000; flashB_ = 22000;  // daylight
      } else {
        flash_ = 0.7f;
        flashR_ = 48000; flashG_ = 18000; flashB_ = 1000;  // amber
      }
    }
    flash_ *= expf(-dt / 0.06f);

    // Playhead: constant time per beat, mapped onto that beat's arc.
    const uint16_t n = strip.size();
    const float f = beatF - (float)beat;
    const uint16_t span = bounds_[beat + 1] - bounds_[beat];
    const uint16_t pos =
        (uint16_t)((float)bounds_[beat] + f * (float)span) % n;

    for (uint16_t i = 0; i < n; ++i) {
      uint32_t r = 500, g = 180, bl = 40;  // faint background

      for (uint8_t k = 0; k < kBeats; ++k) {  // static beat markers
        const uint16_t m = bounds_[k];
        if (i == m || i == (m + 1) % n) {
          if (k == 0) { r += 12000; g += 9000; bl += 5000; }  // bar mark
          else { r += 6000; g += 4500; bl += 2500; }
          break;
        }
      }

      const uint16_t d = pos >= i ? pos - i : (uint16_t)(pos + n - i);
      if (d < 3) {  // playhead with a short tail
        const uint32_t w = 42000 >> d;
        r += w; g += w; bl += w;
      }

      r += (uint32_t)(flash_ * (float)flashR_);
      g += (uint32_t)(flash_ * (float)flashG_);
      bl += (uint32_t)(flash_ * (float)flashB_);
      strip.setPixel(i, r > 65535 ? 65535 : (uint16_t)r,
                     g > 65535 ? 65535 : (uint16_t)g,
                     bl > 65535 ? 65535 : (uint16_t)bl);
    }
  }

private:
  static constexpr uint8_t kBeats = 4;  // one beat per side, so one per corner

  uint16_t bpm_ = 120;
  uint16_t bounds_[kBeats + 1] = {0};
  float barPhase_ = 0.0f;
  int lastBeat_ = -1;
  float flash_ = 0.0f;
  uint16_t flashR_ = 0, flashG_ = 0, flashB_ = 0;
  uint32_t lastMs_ = 0;
  bool haveLast_ = false;
};
