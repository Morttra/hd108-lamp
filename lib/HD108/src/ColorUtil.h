#pragma once
#include <stdint.h>

// h: 0..65535 once around the wheel (0 = red, ~21845 = green, ~43690 = blue),
// s and v: 0..65535. Output is linear (pre-gamma) 16-bit per channel.
inline void hsv2rgb16(uint16_t h, uint16_t s, uint16_t v, uint16_t& r,
                      uint16_t& g, uint16_t& b) {
  if (s == 0) {
    r = g = b = v;
    return;
  }
  const uint32_t h6 = (uint32_t)h * 6;
  const uint8_t sector = h6 >> 16;    // 0..5
  const uint32_t f = h6 & 0xFFFF;     // position within sector
  const uint16_t p = (uint16_t)((uint32_t)v * (65535 - s) / 65535);
  const uint16_t q =
      (uint16_t)((uint32_t)v * (65535 - ((uint32_t)s * f >> 16)) / 65535);
  const uint16_t t = (uint16_t)((uint32_t)v *
                                (65535 - ((uint32_t)s * (65535 - f) >> 16)) /
                                65535);
  switch (sector) {
    case 0:  r = v; g = t; b = p; break;
    case 1:  r = q; g = v; b = p; break;
    case 2:  r = p; g = v; b = t; break;
    case 3:  r = p; g = q; b = v; break;
    case 4:  r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
}

// Linear interpolation between two 16-bit values, frac 0..65535.
inline uint16_t lerp16(uint16_t a, uint16_t b, uint16_t frac) {
  return (uint16_t)(a + (((int64_t)b - a) * frac >> 16));
}
