// HD108 lounge lamp — ESP32 Wrover-E, 340 pixels, WiFi OTA + web UI +
// Home Assistant MQTT.
//
// One hardware SPI data stream drives the lamp: data GPIO23 / clock GPIO18
// (VSPI). The strips facing up and down hang off the same two wires, so
// every pixel index lights the same spot on both.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>

#include "HD108.h"
#include "config.h"

#include "effects/Aurora.h"
#include "effects/Breathe.h"
#include "effects/Candle.h"
#include "effects/Comets.h"
#include "effects/DinnerTimer.h"
#include "effects/Effect.h"
#include "effects/Horror.h"
#include "effects/Metronome.h"
#include "effects/Off.h"
#include "effects/Pomodoro.h"
#include "effects/Race.h"
#include "effects/RainbowCycle.h"
#include "effects/SolidColor.h"
#include "effects/WarmWhite.h"

#include "ReadingZone.h"
#include "hamqtt.h"
#include "webui.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif
#ifndef MQTT_HOST
#define MQTT_HOST ""
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASS ""
#endif

// Fallback access-point password when no secrets.h / WiFi is available.
static const char* kApPassword = "hd108lamp";

// ------------------------------------------------------------------- strip

static HD108 strip(LED_COUNT, PIN_LED_DATA, PIN_LED_CLOCK, LED_SPI_HZ);

// ----------------------------------------------------------------- effects

static Off fxOff;
static WarmWhite fxWarmWhite;
static SolidColor fxSolid;
static DinnerTimer fxTimer;
static Candle fxCandle;
static Breathe fxBreathe;
static Aurora fxAurora;
static RainbowCycle fxRainbow;
static Comets fxComets;
static Race fxRace;
static Horror fxHorror;
static Pomodoro fxPomodoro;
static Metronome fxMetronome;

// "Off" first (the everyday mode). The active mode is persisted by name, so
// this list can be reordered freely.
static Effect* g_effects[] = {
    &fxOff,    &fxWarmWhite, &fxSolid,  &fxTimer, &fxCandle, &fxBreathe,
    &fxAurora, &fxRainbow,   &fxComets, &fxRace,  &fxHorror, &fxPomodoro,
    &fxMetronome,
};
static const size_t kEffectCount = sizeof(g_effects) / sizeof(g_effects[0]);

static size_t modeIndexOf(const Effect* e) {
  for (size_t i = 0; i < kEffectCount; ++i) {
    if (g_effects[i] == e) return i;
  }
  return 0;
}

static size_t modeByName(const char* name) {
  for (size_t i = 0; i < kEffectCount; ++i) {
    if (strcmp(g_effects[i]->name(), name) == 0) return i;
  }
  return 0;
}

// Modes that other controls jump into (color pickers, timer, Aurora palette).
static const size_t kSolidMode = modeIndexOf(&fxSolid);
static const size_t kTimerMode = modeIndexOf(&fxTimer);
static const size_t kAuroraMode = modeIndexOf(&fxAurora);
static const size_t kHorrorMode = modeIndexOf(&fxHorror);

// ------------------------------------------------------------------- state

static Preferences g_prefs;
static size_t g_mode = 0;
static bool g_power = true;
static bool g_blanked = false;  // pixel buffer zeroed while powered off
static volatile bool g_otaActive = false;
static uint32_t g_lastFrame = 0;
static uint32_t g_alertUntil = 0;  // doorbell overlay deadline
static bool g_alertActive = false;
static ReadingZone g_reading;  // reading-light overlay, independent of mode

static void applyMode(size_t i, bool save) {
  g_mode = i < kEffectCount ? i : 0;
  g_effects[g_mode]->begin(strip);
  Serial.printf("mode: %s\n", g_effects[g_mode]->name());
  // Never persist the timer as a boot mode — a power cycle should restore
  // the previous lighting mode, not spontaneously start a countdown. Stored
  // under its own key: NVS refuses to retype the old integer "mode" entry.
  if (save && g_mode != kTimerMode) {
    g_prefs.putString("modeName", g_effects[g_mode]->name());
  }
}

static void applyGain(uint8_t gain, bool save) {
  strip.setGlobalGain(gain);
  if (save) g_prefs.putUChar("gain", strip.globalGain());
}

static void setPower(bool on) {
  g_power = on;
  if (on) {
    // The power-off blank zeroed the pixel buffer; repaint modes that only
    // paint in begin() (repaint() preserves timer/fade progress).
    if (g_blanked) g_effects[g_mode]->repaint(strip);
    g_blanked = false;
  }
}

static void startTimer(uint32_t minutes) {
  fxTimer.start(minutes);
  setPower(true);
  applyMode(kTimerMode, true);
}

static void setBpm(uint16_t bpm) {
  fxMetronome.setBpm(bpm);
  g_prefs.putUShort("bpm", fxMetronome.bpm());
}

// Picking a palette is a "show me" action like picking a color: it switches
// the lamp into Aurora. Changing the speed just tunes whatever is running.
static void setAuroraPalette(uint8_t i) {
  fxAurora.setPalette(i);
  g_prefs.putString("aurPalette", Aurora::paletteName(fxAurora.palette()));
  setPower(true);
  if (g_mode != kAuroraMode) applyMode(kAuroraMode, true);
}

static void setAuroraSpeed(uint16_t percent) {
  fxAurora.setSpeed(percent);
  g_prefs.putUShort("aurSpd", fxAurora.speed());
}

// The Horror colors behave like the solid color picker: set one and the
// lamp shows it.
static void setHorrorStrobe(uint32_t rgb) {
  fxHorror.setStrobe(rgb);
  g_prefs.putUInt("horStrobe", fxHorror.strobe());
  setPower(true);
  if (g_mode != kHorrorMode) applyMode(kHorrorMode, true);
}

static void setHorrorBackground(uint32_t rgb) {
  fxHorror.setBackground(rgb);
  g_prefs.putUInt("horBg", fxHorror.background());
  setPower(true);
  if (g_mode != kHorrorMode) applyMode(kHorrorMode, true);
}

static void triggerAlert() {
  g_alertUntil = millis() + ALERT_MS;
  g_alertActive = true;
}

// --------------------------------------------------------- reading light

// The zone moved or went away: the pixels it covered go back to the mode's
// own colors and the global gain.
static void readingRepaint() {
  g_reading.release(strip);
  if (g_power) {
    g_effects[g_mode]->repaint(strip);
  } else {
    g_blanked = false;
  }
}

static void setReadingOn(bool on) {
  if (on == g_reading.enabled()) return;
  g_reading.setEnabled(on);
  g_prefs.putBool("rdOn", on);
  if (!on) readingRepaint();
}

// Aiming or dimming the reading light also switches it on — you want to see
// what you are adjusting. `save` is false while a slider is being dragged.
static void setReadingPosition(uint16_t pos, bool save) {
  g_reading.setPosition(pos);
  if (save) g_prefs.putUShort("rdPos", g_reading.position());
  readingRepaint();
  setReadingOn(true);
}

static void setReadingGain(uint8_t gain) {
  g_reading.setGain(gain);
  g_prefs.putUChar("rdGain", g_reading.gain());
  setReadingOn(true);
}

// ---------------------------------------------------------------- network

static void setupWifi() {
  WiFi.setHostname(HOSTNAME);  // must precede mode(): applied on NULL->STA
  WiFi.mode(WIFI_STA);
  if (strlen(WIFI_SSID) > 0) {
    Serial.printf("wifi: connecting to \"%s\"", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();
  }
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.setAutoReconnect(true);
    Serial.printf("wifi: connected, http://%s.local/ (%s)\n", HOSTNAME,
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.printf(
        "wifi: not connected, starting AP \"%s\" (pw \"%s\"), UI at "
        "http://192.168.4.1/\n",
        HOSTNAME, kApPassword);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(HOSTNAME, kApPassword);
  }
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  }
}

static void setupOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    g_otaActive = true;
    strip.clear();
    strip.show();
    Serial.println("ota: start");
  });
  // Progress bar on the lamp itself: the perimeter fills with dim blue.
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    if (total == 0) return;
    const uint16_t lit = (uint16_t)((uint64_t)strip.size() * done / total);
    for (uint16_t i = 0; i < strip.size(); ++i) {
      strip.setPixel(i, 0, 0, i < lit ? 6000 : 0);
    }
    strip.show();
  });
  ArduinoOTA.onEnd([]() {
    strip.clear();
    strip.show();
    Serial.println("ota: done, rebooting");
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("ota: error %u\n", (unsigned)e);
    g_otaActive = false;
    g_effects[g_mode]->repaint(strip);
  });
  ArduinoOTA.begin();
}

static void setupWifiServices() {
  setupWifi();
  setupOta();

  webui::Api api;
  api.effectCount = []() { return kEffectCount; };
  api.effectName = [](size_t i) { return g_effects[i]->name(); };
  api.mode = []() { return g_mode; };
  api.setMode = [](size_t i) {
    setPower(true);
    applyMode(i, true);
    hamqtt::publishState();
  };
  api.gain = []() { return strip.globalGain(); };
  api.setGain = [](uint8_t g) {
    applyGain(g, true);
    hamqtt::publishState();
  };
  api.timerMinutes = []() { return fxTimer.minutes(); };
  api.startTimer = [](uint32_t m) {
    startTimer(m);
    hamqtt::publishState();
  };
  api.doorbell = []() { triggerAlert(); };
  api.bpm = []() { return fxMetronome.bpm(); };
  api.setBpm = [](uint16_t b) {
    setBpm(b);
    hamqtt::publishState();
  };
  api.solidColor = []() {
    return ((uint32_t)fxSolid.red() << 16) | ((uint32_t)fxSolid.green() << 8) |
           fxSolid.blue();
  };
  api.setColor = [](uint8_t r, uint8_t g, uint8_t b) {
    fxSolid.set(r, g, b);
    setPower(true);
    if (g_mode != kSolidMode) applyMode(kSolidMode, true);
    hamqtt::publishState();
  };
  api.auroraPaletteCount = []() { return (size_t)Aurora::kPaletteCount; };
  api.auroraPaletteName = [](size_t i) {
    return Aurora::paletteName((uint8_t)i);
  };
  api.auroraPalette = []() { return (size_t)fxAurora.palette(); };
  api.setAuroraPalette = [](size_t i) {
    setAuroraPalette((uint8_t)i);
    hamqtt::publishState();
  };
  api.auroraSpeed = []() { return fxAurora.speed(); };
  api.setAuroraSpeed = [](uint16_t p) {
    setAuroraSpeed(p);
    hamqtt::publishState();
  };
  api.horrorStrobe = []() { return fxHorror.strobe(); };
  api.horrorBackground = []() { return fxHorror.background(); };
  api.setHorrorStrobe = [](uint32_t rgb) {
    setHorrorStrobe(rgb);
    hamqtt::publishState();
  };
  api.setHorrorBackground = [](uint32_t rgb) {
    setHorrorBackground(rgb);
    hamqtt::publishState();
  };
  api.readingOn = []() { return g_reading.enabled(); };
  api.setReadingOn = [](bool on) {
    setReadingOn(on);
    hamqtt::publishState();
  };
  api.readingPosition = []() { return g_reading.position(); };
  api.setReadingPosition = [](uint16_t p, bool save) {
    setReadingPosition(p, save);
    if (save) hamqtt::publishState();  // not at slider-drag rate
  };
  api.readingGain = []() { return g_reading.gain(); };
  api.setReadingGain = [](uint8_t g) {
    setReadingGain(g);
    hamqtt::publishState();
  };
  webui::begin(api);

  hamqtt::Api ha;
  ha.effectCount = []() { return kEffectCount; };
  ha.effectName = [](size_t i) { return g_effects[i]->name(); };
  ha.mode = []() { return g_mode; };
  ha.setMode = [](size_t i) { applyMode(i, true); };
  ha.gain = []() { return strip.globalGain(); };
  ha.setGain = [](uint8_t g) { applyGain(g, true); };
  ha.power = []() { return g_power; };
  ha.setPower = [](bool on) { setPower(on); };
  ha.setColor = [](uint8_t r, uint8_t g, uint8_t b) {
    fxSolid.set(r, g, b);
    if (g_mode != kSolidMode) applyMode(kSolidMode, true);
  };
  ha.timerMinutes = []() { return fxTimer.minutes(); };
  ha.startTimer = [](uint32_t m) { startTimer(m); };
  ha.doorbell = []() { triggerAlert(); };
  ha.bpm = []() { return fxMetronome.bpm(); };
  ha.setBpm = [](uint16_t b) { setBpm(b); };
  ha.auroraPaletteCount = []() { return (size_t)Aurora::kPaletteCount; };
  ha.auroraPaletteName = [](size_t i) {
    return Aurora::paletteName((uint8_t)i);
  };
  ha.auroraPalette = []() { return (size_t)fxAurora.palette(); };
  ha.setAuroraPalette = [](size_t i) { setAuroraPalette((uint8_t)i); };
  ha.auroraSpeed = []() { return fxAurora.speed(); };
  ha.setAuroraSpeed = [](uint16_t p) { setAuroraSpeed(p); };
  ha.readingOn = []() { return g_reading.enabled(); };
  ha.setReadingOn = [](bool on) { setReadingOn(on); };
  ha.readingPosition = []() { return g_reading.position(); };
  ha.setReadingPosition = [](uint16_t p) { setReadingPosition(p, true); };
  ha.readingGain = []() { return g_reading.gain(); };
  ha.setReadingGain = [](uint8_t g) { setReadingGain(g); };
  if (WiFi.status() == WL_CONNECTED) {
    hamqtt::begin(ha, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS);
  } else {
    // In AP fallback the broker is unreachable and each reconnect attempt
    // would block the render loop for seconds.
    Serial.println("mqtt: skipped (no station connection)");
  }
}

// ------------------------------------------------------------------- boot

void setup() {
  Serial.begin(115200);
  Serial.println("\nHD108 lamp booting");

  if (!strip.begin()) Serial.println("FATAL: LED buffer allocation failed");
  strip.setColorOrder(LED_COLOR_ORDER);
  strip.setOffset(LED_OFFSET);

  g_prefs.begin("lamp");
  applyGain(g_prefs.getUChar("gain", LED_DEFAULT_GAIN), false);
  fxMetronome.setBpm(g_prefs.getUShort("bpm", 120));
  fxAurora.setPalette(
      Aurora::paletteByName(g_prefs.getString("aurPalette", "").c_str()));
  fxAurora.setSpeed(g_prefs.getUShort("aurSpd", 100));
  fxHorror.setStrobe(g_prefs.getUInt("horStrobe", fxHorror.strobe()));
  fxHorror.setBackground(g_prefs.getUInt("horBg", fxHorror.background()));
  g_reading.setPosition(g_prefs.getUShort("rdPos", SIDE0_COUNT / 2));
  g_reading.setGain(g_prefs.getUChar("rdGain", g_reading.gain()));
  g_reading.setEnabled(g_prefs.getBool("rdOn", false));

  setupWifiServices();

  // Fresh installs boot into Off; afterwards the last used mode is restored.
  applyMode(modeByName(g_prefs.getString("modeName", "").c_str()), false);
}

// ------------------------------------------------------------------- loop

static void renderFrame(uint32_t now) {
  if (g_alertActive) {
    if ((int32_t)(g_alertUntil - now) > 0) {
      // Doorbell overlay: urgent blue pulsing, ~2 Hz.
      const float ph = (float)(now % 500u) / 500.0f;
      const float v = 0.25f + 0.75f * (0.5f - 0.5f * cosf(TWO_PI * ph));
      strip.fill((uint16_t)(v * 4000.0f), (uint16_t)(v * 14000.0f),
                 (uint16_t)(v * 62000.0f));
      if (g_reading.enabled()) g_reading.apply(strip);  // reader keeps light
      strip.show();
    } else {
      g_alertActive = false;
      if (g_power) {
        g_effects[g_mode]->repaint(strip);  // restore, don't reset state
      } else {
        g_blanked = false;  // force a re-blank on the next frame
      }
    }
    return;
  }

  if (g_power) {
    g_effects[g_mode]->update(strip, now);
  } else if (g_blanked && !g_reading.enabled()) {
    return;  // already dark; nothing changes until something switches on
  } else {
    strip.clear();  // lamp off: black, with the reading light on top if on
    g_blanked = true;
  }
  // The reading light paints over whatever is underneath, every frame.
  if (g_reading.enabled()) g_reading.apply(strip);
  strip.show();
}

void loop() {
  const uint32_t now = millis();

  ArduinoOTA.handle();
  if (g_otaActive) {
    delay(1);
    return;
  }
  webui::handle();
  hamqtt::loop();

  // Frame pacing: fast effects may ask for a shorter interval than default.
  uint16_t frameMs = FRAME_MS;
  if (g_power && g_effects[g_mode]->frameMs() < frameMs) {
    frameMs = g_effects[g_mode]->frameMs();
  }
  if (now - g_lastFrame >= frameMs) {
    g_lastFrame = now;
    renderFrame(now);
  }
  delay(1);
}
