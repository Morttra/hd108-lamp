#pragma once
#include <Arduino.h>

#include "HD108.h"
#include "config.h"

// Reading light: a warm-white arc painted over whatever the lamp is doing —
// any mode, the doorbell alert, even the lamp switched "off". It is drawn
// after the mode renders each frame, so switching it off just lets the
// mode's own pixels show again (the main loop repaints them).
//
// The arc runs at its own HD108 current gain, independent of the lamp's
// global brightness: the room can sit at a dim evening gain while the
// reading spot runs near full current. Position is the logical LED index of
// the arc's center (0 = the first corner, following the side counts in
// config.h) — the same number a map picker in Home Assistant would send.
//
// Tuning:
//   kHalfWidth — LEDs each side of the center (18 ≈ 75 cm in total).
//   kFeather — LEDs of fade-out at each end.
//   kColor — the warm white in 8-bit sRGB terms; lower the blue for warmer.
class ReadingZone {
public:
  static constexpr uint16_t kHalfWidth = 18;
  static constexpr uint16_t kFeather = 5;

  ReadingZone()
      : linR_(HD108Strip::gamma16(kColor[0])),
        linG_(HD108Strip::gamma16(kColor[1])),
        linB_(HD108Strip::gamma16(kColor[2])) {}

  bool enabled() const { return enabled_; }
  void setEnabled(bool on) { enabled_ = on; }

  uint16_t position() const { return position_; }
  void setPosition(uint16_t p) {
    position_ = p < LED_COUNT ? p : LED_COUNT - 1;
  }

  uint8_t gain() const { return gain_; }
  void setGain(uint8_t g) { gain_ = g < 1 ? 1 : g > 31 ? 31 : g; }

  // Paints the arc over the strip's current frame: call after the mode's
  // update() and before show().
  void apply(HD108Strip& strip) const {
    const int n = strip.size();
    if (n == 0) return;
    for (int d = -(int)kHalfWidth; d <= (int)kHalfWidth; ++d) {
      const uint16_t i = (uint16_t)((((int)position_ + d) % n + n) % n);
      const int edge = (int)kHalfWidth - abs(d);  // 0 at the outermost LED
      const float w = edge < (int)kFeather
                          ? (float)(edge + 1) / (float)(kFeather + 1)
                          : 1.0f;
      strip.setPixel(i, (uint16_t)(linR_ * w), (uint16_t)(linG_ * w),
                     (uint16_t)(linB_ * w));
      strip.setPixelGain(i, gain_);
    }
  }

  // Hands the arc's pixels back: resets every pixel to the global gain. The
  // caller repaints the mode's colors.
  void release(HD108Strip& strip) const {
    strip.setGlobalGain(strip.globalGain());
  }

private:
  static constexpr uint8_t kColor[3] = {255, 160, 70};  // ~3000 K warm white

  bool enabled_ = false;
  uint16_t position_ = 0;
  uint8_t gain_ = 24;
  float linR_, linG_, linB_;
};
