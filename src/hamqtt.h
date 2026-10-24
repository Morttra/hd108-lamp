#pragma once
#include <Arduino.h>

#include <functional>

// Home Assistant integration over MQTT with auto-discovery (ArduinoHA).
// Entities in HA: a light (on/off, brightness -> current gain, RGB -> solid
// color), a mode select, a timer-minutes number + start button, a metronome
// BPM number, an Aurora palette select + speed number, the reading light
// (a second light: on/off + its own brightness) with its position number,
// and a "Doorbell alert" button HA automations can press to flash the
// lamp. Skipped entirely when no broker host is configured.
namespace hamqtt {

struct Api {
  std::function<size_t()> effectCount;
  std::function<const char*(size_t)> effectName;
  std::function<size_t()> mode;
  std::function<void(size_t)> setMode;
  std::function<uint8_t()> gain;  // 0..31
  std::function<void(uint8_t)> setGain;
  std::function<bool()> power;
  std::function<void(bool)> setPower;
  std::function<void(uint8_t, uint8_t, uint8_t)> setColor;
  std::function<uint32_t()> timerMinutes;
  std::function<void(uint32_t)> startTimer;  // minutes
  std::function<void()> doorbell;            // transient alert overlay
  std::function<uint16_t()> bpm;             // metronome tempo
  std::function<void(uint16_t)> setBpm;
  std::function<size_t()> auroraPaletteCount;
  std::function<const char*(size_t)> auroraPaletteName;
  std::function<size_t()> auroraPalette;
  std::function<void(size_t)> setAuroraPalette;  // also switches to Aurora
  std::function<uint16_t()> auroraSpeed;         // percent, 10..300
  std::function<void(uint16_t)> setAuroraSpeed;
  std::function<bool()> readingOn;
  std::function<void(bool)> setReadingOn;
  std::function<uint16_t()> readingPosition;  // LED index around the ring
  std::function<void(uint16_t)> setReadingPosition;  // also switches it on
  std::function<uint8_t()> readingGain;              // 1..31
  std::function<void(uint8_t)> setReadingGain;       // also switches it on
};

void begin(const Api& api, const char* host, uint16_t port, const char* user,
           const char* pass);
void loop();
// Push current lamp state to HA (call after web-UI-side changes).
void publishState();

}  // namespace hamqtt
