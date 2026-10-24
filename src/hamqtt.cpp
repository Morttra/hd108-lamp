#include "hamqtt.h"

#include <ArduinoHA.h>
#include <WiFi.h>

#include "config.h"

namespace hamqtt {
namespace {

Api g_api;
bool g_active = false;

WiFiClient g_net;
HADevice g_device;
HAMqtt g_mqtt(g_net, g_device, 12);

constexpr uint8_t kLightFeatures =
    HALight::BrightnessFeature | HALight::RGBFeature;
HALight g_light("hd108_light", kLightFeatures);
HASelect g_select("hd108_mode");
HANumber g_timerMin("hd108_timer_minutes", HABaseDeviceType::PrecisionP0);
HANumber g_bpm("hd108_metronome_bpm", HABaseDeviceType::PrecisionP0);
HASelect g_auroraPalette("hd108_aurora_palette");
HANumber g_auroraSpeed("hd108_aurora_speed", HABaseDeviceType::PrecisionP0);
HALight g_reading("hd108_reading", HALight::BrightnessFeature);
HANumber g_readingPos("hd108_reading_pos", HABaseDeviceType::PrecisionP0);
HAButton g_timerStart("hd108_timer_start");
HAButton g_bell("hd108_doorbell");

String g_modeOptions, g_paletteOptions;  // must outlive setOptions()

uint8_t gainToBrightness(uint8_t gain) {
  return (uint8_t)(((uint16_t)gain * 255 + 15) / 31);
}
uint8_t brightnessToGain(uint8_t b) {
  uint8_t gain = (uint8_t)(((uint16_t)b * 31 + 127) / 255);
  return gain == 0 ? 1 : gain;
}

void onLightState(bool state, HALight* sender) {
  g_api.setPower(state);
  sender->setState(state, true);
  publishState();
}

void onLightBrightness(uint8_t b, HALight* sender) {
  g_api.setGain(brightnessToGain(b));
  if (!g_api.power()) g_api.setPower(true);
  sender->setBrightness(b, true);
  publishState();
}

void onLightRGB(HALight::RGBColor color, HALight* sender) {
  g_api.setColor(color.red, color.green, color.blue);
  if (!g_api.power()) g_api.setPower(true);
  sender->setRGBColor(color, true);
  publishState();
}

void onSelect(int8_t index, HASelect*) {
  if (index >= 0 && (size_t)index < g_api.effectCount()) {
    g_api.setMode((size_t)index);
    if (!g_api.power()) g_api.setPower(true);
  }
  publishState();
}

void onTimerMinutes(HANumeric value, HANumber* sender) {
  if (value.isSet()) {
    const uint32_t m = value.toUInt32();
    if (m >= 1 && m <= 720) {
      g_api.startTimer(m);  // setting the number arms + starts the timer
    }
  }
  sender->setState(value, true);
  publishState();
}

void onBpm(HANumeric value, HANumber* sender) {
  if (value.isSet()) {
    const uint32_t b = value.toUInt32();
    if (b >= 30 && b <= 240) g_api.setBpm((uint16_t)b);
  }
  sender->setState(value, true);
  publishState();
}

void onAuroraPalette(int8_t index, HASelect*) {
  if (index >= 0 && (size_t)index < g_api.auroraPaletteCount()) {
    g_api.setAuroraPalette((size_t)index);
  }
  publishState();
}

void onAuroraSpeed(HANumeric value, HANumber* sender) {
  if (value.isSet()) {
    const uint32_t p = value.toUInt32();
    if (p >= 10 && p <= 300) g_api.setAuroraSpeed((uint16_t)p);
  }
  sender->setState(value, true);
  publishState();
}

void onReadingState(bool state, HALight* sender) {
  g_api.setReadingOn(state);
  sender->setState(state, true);
  publishState();
}

void onReadingBrightness(uint8_t b, HALight* sender) {
  g_api.setReadingGain(brightnessToGain(b));
  sender->setBrightness(b, true);
  publishState();
}

void onReadingPos(HANumeric value, HANumber* sender) {
  if (value.isSet()) {
    const uint32_t p = value.toUInt32();
    if (p < LED_COUNT) g_api.setReadingPosition((uint16_t)p);
  }
  sender->setState(value, true);
  publishState();
}

void onTimerStart(HAButton*) {
  g_api.startTimer(g_api.timerMinutes());
  publishState();
}

void onBell(HAButton*) { g_api.doorbell(); }

// Semicolon-separated option list for a select entity.
void joinNames(String& out, size_t n,
               const std::function<const char*(size_t)>& name) {
  out = "";
  out.reserve(180);
  for (size_t i = 0; i < n; ++i) {
    if (i) out += ';';
    out += name(i);
  }
}

}  // namespace

void begin(const Api& api, const char* host, uint16_t port, const char* user,
           const char* pass) {
  if (host == nullptr || strlen(host) == 0) {
    Serial.println("mqtt: no broker configured, skipping");
    return;
  }
  g_api = api;

  static byte mac[6];
  WiFi.macAddress(mac);
  g_device.setUniqueId(mac, sizeof(mac));
  g_device.setName("HD108 Lamp");
  g_device.setModel("HD108 340px");
  g_device.setManufacturer("Morten");
  g_device.enableSharedAvailability();
  g_device.enableLastWill();

  g_light.setName("Light");
  g_light.onStateCommand(onLightState);
  g_light.onBrightnessCommand(onLightBrightness);
  g_light.onRGBColorCommand(onLightRGB);

  joinNames(g_modeOptions, g_api.effectCount(), g_api.effectName);
  g_select.setName("Mode");
  g_select.setOptions(g_modeOptions.c_str());
  g_select.onCommand(onSelect);

  g_timerMin.setName("Dinner timer");
  g_timerMin.setUnitOfMeasurement("min");
  g_timerMin.setMin(1);
  g_timerMin.setMax(720);
  g_timerMin.setStep(1);
  g_timerMin.onCommand(onTimerMinutes);

  g_bpm.setName("Metronome BPM");
  g_bpm.setIcon("mdi:metronome");
  g_bpm.setUnitOfMeasurement("bpm");
  g_bpm.setMin(30);
  g_bpm.setMax(240);
  g_bpm.setStep(1);
  g_bpm.onCommand(onBpm);

  joinNames(g_paletteOptions, g_api.auroraPaletteCount(),
            g_api.auroraPaletteName);
  g_auroraPalette.setName("Aurora palette");
  g_auroraPalette.setIcon("mdi:palette");
  g_auroraPalette.setOptions(g_paletteOptions.c_str());
  g_auroraPalette.onCommand(onAuroraPalette);

  g_auroraSpeed.setName("Aurora speed");
  g_auroraSpeed.setIcon("mdi:speedometer");
  g_auroraSpeed.setUnitOfMeasurement("%");
  g_auroraSpeed.setMin(10);
  g_auroraSpeed.setMax(300);
  g_auroraSpeed.setStep(10);
  g_auroraSpeed.onCommand(onAuroraSpeed);

  g_reading.setName("Reading light");
  g_reading.setIcon("mdi:book-open-page-variant");
  g_reading.onStateCommand(onReadingState);
  g_reading.onBrightnessCommand(onReadingBrightness);

  g_readingPos.setName("Reading position");
  g_readingPos.setIcon("mdi:map-marker");
  g_readingPos.setMin(0);
  g_readingPos.setMax(LED_COUNT - 1);
  g_readingPos.setStep(1);
  g_readingPos.onCommand(onReadingPos);

  g_timerStart.setName("Start dinner timer");
  g_timerStart.setIcon("mdi:timer-play");
  g_timerStart.onCommand(onTimerStart);

  g_bell.setName("Doorbell alert");
  g_bell.setIcon("mdi:bell-ring");
  g_bell.onCommand(onBell);

  // Connection happens inside loop(), not begin() — publish real state on
  // every (re)connect so HA never sees the library's defaults (off/0/None).
  g_mqtt.onConnected([]() { publishState(); });

  const bool started =
      strlen(user) > 0 ? g_mqtt.begin(host, port, user, pass)
                       : g_mqtt.begin(host, port);
  g_active = started;
  Serial.printf("mqtt: %s -> %s:%u\n",
                started ? "connecting" : "failed to start", host, port);
}

void loop() {
  if (g_active) g_mqtt.loop();
}

void publishState() {
  if (!g_active) return;
  g_light.setState(g_api.power());
  g_light.setBrightness(gainToBrightness(g_api.gain()));
  g_select.setState((int8_t)g_api.mode());
  const HANumeric minutes((uint32_t)g_api.timerMinutes(), 0);
  g_timerMin.setState(minutes);
  const HANumeric bpmVal((uint32_t)g_api.bpm(), 0);
  g_bpm.setState(bpmVal);
  g_auroraPalette.setState((int8_t)g_api.auroraPalette());
  const HANumeric speed((uint32_t)g_api.auroraSpeed(), 0);
  g_auroraSpeed.setState(speed);
  g_reading.setState(g_api.readingOn());
  g_reading.setBrightness(gainToBrightness(g_api.readingGain()));
  const HANumeric pos((uint32_t)g_api.readingPosition(), 0);
  g_readingPos.setState(pos);
}

}  // namespace hamqtt
