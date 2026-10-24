#pragma once
#include "Effect.h"

// Off: the lamp is dark but stays powered and connected — web UI, Home
// Assistant, OTA and the doorbell alert all keep working. The alert overlay
// draws over any mode and restores it afterward, so a doorbell still flashes
// the lamp out of Off and it goes dark again by itself.
class Off : public Effect {
public:
  const char* name() const override { return "Off"; }

  // Effect::begin() clears the strip; nothing to animate.
  void update(HD108&, uint32_t) override {}
};
