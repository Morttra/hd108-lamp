#pragma once
#include <Arduino.h>

#include "config.h"

// Maps an LED index (0..LED_COUNT-1) to its physical position on the
// rectangular loop. Side 0 starts at the corner where the data line enters
// and sides follow the physical data direction; calibrate the per-side
// counts in config.h.

struct PointF {
  float x, y;
};

namespace geo {

constexpr uint16_t kSideCount[4] = {SIDE0_COUNT, SIDE1_COUNT, SIDE2_COUNT,
                                    SIDE3_COUNT};

static_assert(SIDE0_COUNT + SIDE1_COUNT + SIDE2_COUNT + SIDE3_COUNT ==
                  LED_COUNT,
              "side counts in config.h must sum to LED_COUNT");

// Fraction 0..1 of the way around the perimeter.
inline float frac(uint16_t i) { return (float)i / (float)LED_COUNT; }

// Side index 0..3; optionally the 0..1 position along that side.
inline uint8_t side(uint16_t i, float* t = nullptr) {
  uint16_t rem = i;
  for (uint8_t s = 0; s < 4; ++s) {
    if (rem < kSideCount[s]) {
      if (t) {
        *t = kSideCount[s] > 1 ? (float)rem / (float)(kSideCount[s] - 1)
                               : 0.0f;
      }
      return s;
    }
    rem -= kSideCount[s];
  }
  if (t) *t = 0.0f;
  return 0;
}

// Position in meters, origin at the rectangle's center.
inline PointF xy(uint16_t i) {
  float t;
  const uint8_t s = side(i, &t);
  const float w = RECT_W_M * 0.5f;
  const float h = RECT_H_M * 0.5f;
  switch (s) {
    case 0:  return {-w + t * RECT_W_M, -h};
    case 1:  return {w, -h + t * RECT_H_M};
    case 2:  return {w - t * RECT_W_M, h};
    default: return {-w, h - t * RECT_H_M};
  }
}

// Angle from the lamp's center, radians in [-pi, pi]. Useful for sweeps.
inline float angle(uint16_t i) {
  const PointF p = xy(i);
  return atan2f(p.y, p.x);
}

}  // namespace geo
