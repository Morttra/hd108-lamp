#pragma once
#include <Arduino.h>

#include <functional>

// Minimal phone-friendly web UI + JSON API on port 80.
//   GET /                    control page
//   GET /api/status          {"timer":n,"bpm":n,"mode":n,"gain":n,"color":"#rrggbb",
//                             "effects":[...],
//                             "aurora":{"palette":n,"speed":p,"palettes":[...]},
//                             "horror":{"strobe":"#rrggbb","bg":"#rrggbb"},
//                             "leds":340,"reading":{"on":b,"pos":n,"gain":n}}
//   GET /api/mode?i=N        switch mode
//   GET /api/gain?v=N        brightness, 0..31
//   GET /api/timer?min=N     start the dinner timer (1..720 minutes)
//   GET /api/color?hex=RRGGBB  set solid color (switches to Solid mode)
//   GET /api/bpm?v=N         set metronome tempo (30..240)
//   GET /api/aurora?palette=N&speed=P  Aurora palette index (switches to
//                            Aurora) and/or drift speed in percent (10..300)
//   GET /api/horror?strobe=RRGGBB&bg=RRGGBB  Horror flash and/or background
//                            color (switches to Horror)
//   GET /api/reading?on=1|0&pos=N&gain=N  reading light: toggle, aim (LED
//                            index; add live=1 while dragging to skip the
//                            flash write), brightness 1..31. Aiming or
//                            dimming also switches it on.
//   GET /api/bell            trigger the doorbell alert overlay (for testing)
namespace webui {

struct Api {
  std::function<size_t()> effectCount;
  std::function<const char*(size_t)> effectName;
  std::function<size_t()> mode;
  std::function<void(size_t)> setMode;
  std::function<uint8_t()> gain;
  std::function<void(uint8_t)> setGain;
  std::function<uint32_t()> timerMinutes;
  std::function<void(uint32_t)> startTimer;
  std::function<void()> doorbell;
  std::function<uint32_t()> solidColor;  // 0xRRGGBB
  std::function<void(uint8_t, uint8_t, uint8_t)> setColor;
  std::function<uint16_t()> bpm;
  std::function<void(uint16_t)> setBpm;
  std::function<size_t()> auroraPaletteCount;
  std::function<const char*(size_t)> auroraPaletteName;
  std::function<size_t()> auroraPalette;
  std::function<void(size_t)> setAuroraPalette;
  std::function<uint16_t()> auroraSpeed;  // percent
  std::function<void(uint16_t)> setAuroraSpeed;
  std::function<uint32_t()> horrorStrobe;      // 0xRRGGBB
  std::function<uint32_t()> horrorBackground;  // 0xRRGGBB
  std::function<void(uint32_t)> setHorrorStrobe;
  std::function<void(uint32_t)> setHorrorBackground;
  std::function<bool()> readingOn;
  std::function<void(bool)> setReadingOn;
  std::function<uint16_t()> readingPosition;
  std::function<void(uint16_t, bool)> setReadingPosition;  // pos, persist
  std::function<uint8_t()> readingGain;
  std::function<void(uint8_t)> setReadingGain;
};

void begin(const Api& api);
void handle();

}  // namespace webui
