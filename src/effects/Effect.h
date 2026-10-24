#pragma once
#include <Arduino.h>

#include "HD108.h"
#include "config.h"

class Effect {
public:
  virtual ~Effect() = default;
  virtual const char* name() const = 0;
  // Frame interval this effect wants; fast effects can override to run the
  // render loop quicker than the default.
  virtual uint16_t frameMs() const { return FRAME_MS; }
  // Called once when the mode becomes active.
  virtual void begin(HD108& strip) { strip.clear(); }
  // Called once per frame; write pixels, main loop calls show().
  virtual void update(HD108& strip, uint32_t nowMs) = 0;
  // Repaint the buffer WITHOUT resetting effect state (used after overlays
  // and power-on). Defaults to begin(); stateful effects that redraw every
  // frame anyway (timer, sun fade) override this as a no-op.
  virtual void repaint(HD108& strip) { begin(strip); }
};
