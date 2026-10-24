# HD108 Lounge Lamp

Firmware for a large pendant lamp built from **HD108** 16-bit RGB LED strips
and an **ESP32**. It was written for a 2.0 m × 1.2 m rectangular spruce
frame with one strip facing down (direct light) and one facing up (ceiling
wash), but nothing depends on that shape: set the LED count, pins and side
lengths in one header and it runs any HD108 loop or strip.

What it does:

- **13 light modes** — warm white, solid color, candle, aurora with
  palettes and adjustable speed, rainbow, comets, race, a horror-corridor
  flicker, a 4-7-8 breathing guide, a dinner timer, a pomodoro timer and a
  4/4 visual metronome.
- A **reading light**: a warm-white arc you aim anywhere on the lamp, with
  its own brightness, drawn over whatever else is running (or over the lamp
  being off). It uses the HD108's per-pixel current gain, so the room can
  be dim while the reading spot is bright.
- A phone-friendly **web page** and a JSON API served by the lamp.
- **Home Assistant** via MQTT auto-discovery: light, mode select, reading
  light, timers, metronome tempo, aurora settings, doorbell alert.
- **OTA updates** with a progress bar on the lamp itself.
- Dimming by LED current rather than PWM, so it never flickers, and 16-bit
  fades that stay smooth where 8-bit strips step.

## Hardware

The parts used here, as a worked example:

| Part | This lamp | Notes |
|---|---|---|
| LED strips | HD108, 340 px per strip, two strips wired in parallel | One strip works the same; set `LED_COUNT` |
| Controller | ESP32-WROVER-E dev board (`esp32dev`) | Any ESP32 with free VSPI pins |
| Power | 5 V, ~50 A supply, injected at 6 points through 6 mm² leads | See the power note |
| Data/clock | ~33 Ω series resistors near the first LED | Datasheet recommendation |

Wiring:

| Signal | ESP32 pin | Notes |
|---|---|---|
| LED data (SDI) | GPIO 23 | VSPI MOSI, hardware SPI |
| LED clock (CKI) | GPIO 18 | VSPI SCLK |
| Strip GND | GND | Common ground with the ESP32 |

- Logic is 3.3 V into a 5 V-supplied strip. That works on most strips; if
  you see glitches at higher SPI clocks, add a level shifter (74AHCT125 or
  similar) or lower `LED_SPI_HZ` in `include/config.h`.
- The datasheet also recommends 27–100 Ω in series with VDD close to the
  first LED.
- **Power**: an HD108 pixel draws about 0.26 W at full white and full
  current gain, so 680 LEDs is roughly 175 W / 35 A at 5 V worst case.
  Size the supply for it and inject power every metre or two; thin feeds
  sag and the far end turns yellow. `LED_DEFAULT_GAIN` in `config.h` only
  sets where the brightness slider starts. It is not a safety limit.

## Build and flash

Needs [PlatformIO](https://platformio.org/), either the VS Code extension
or the `pio` command line. The project uses the community
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform
for Arduino core 3.x; PlatformIO downloads it on the first build.

1. `cp include/secrets.h.example include/secrets.h` and fill in WiFi, an
   OTA password and, if you use Home Assistant, the MQTT broker.
2. First flash over USB: `pio run -e lamp -t upload`
3. For OTA from then on: `cp platformio_local.ini.example
   platformio_local.ini` and put the same OTA password there. The file is
   gitignored. With it in place `pio run -e lamp -t upload` flashes over
   WiFi; add `--upload-port <ip>` if mDNS is unreliable on your network.
   `pio run -e lamp-usb -t upload` always goes over USB.
4. Open `http://hd108-lamp.local/`. Without WiFi credentials the lamp
   starts its own access point (`hd108-lamp` / `hd108lamp`, page at
   `http://192.168.4.1/`).

During an OTA update the lamp goes dark and a blue bar fills around the
perimeter, then it reboots into the new firmware.

## Fit it to your lamp

Everything lamp-specific is in `include/config.h`:

- `LED_COUNT`, the data and clock pins, `LED_SPI_HZ`.
- `LED_COLOR_ORDER`: the datasheet says BGR on the wire, but batches
  differ (these strips are RGB). Pick pure red in the web page's color
  picker; if the lamp shows green or blue, change the order.
- `SIDE0_COUNT` to `SIDE3_COUNT`: LEDs per side of the rectangle in data
  order, summing to `LED_COUNT`. Position-aware code (the metronome's
  corner beats, `geo::xy()`) uses them. For a ring or a straight strip,
  split the count four ways; only the corner markers care.
- `LED_OFFSET`: rotates the ring so logical LED 0 sits on a landmark, a
  corner here. The metronome's four beat markers land exactly on the
  corners when the side counts and offset are right.
- `LED_DEFAULT_GAIN`, `FRAME_MS`, `ALERT_MS`, `HOSTNAME`.

## Using it

The web page lists the modes, a brightness slider (0–31, the LED current
gain), a solid-color picker, the reading light, the aurora and horror
settings, the dinner timer and the metronome tempo. The same things over
HTTP:

| Request | Effect |
|---|---|
| `GET /api/status` | JSON: mode list, current mode, gain, colors, timer, bpm, aurora, horror, reading |
| `GET /api/mode?i=N` | switch mode |
| `GET /api/gain?v=N` | brightness 0..31 |
| `GET /api/color?hex=RRGGBB` | solid color (switches to Solid color) |
| `GET /api/timer?min=N` | start the dinner timer, 1..720 min |
| `GET /api/bpm?v=N` | metronome tempo 30..240 |
| `GET /api/aurora?palette=N&speed=P` | aurora palette (switches to Aurora) and/or speed 10..300 % |
| `GET /api/horror?strobe=RRGGBB&bg=RRGGBB` | horror flash and background colors (switches to Horror) |
| `GET /api/reading?on=1\|0&pos=N&gain=N` | reading light toggle, position (LED index), brightness 1..31 |
| `GET /api/bell` | fire the doorbell alert |

The modes:

| Mode | What it does |
|---|---|
| Off | Dark but connected: web page, HA, OTA, doorbell and reading light keep working |
| Warm white | Static, about 2700 K |
| Solid color | Any color from the picker or Home Assistant |
| Dinner timer | Amber arc counts down around the ring, the last minute pulses, green breathing when done |
| Candle | Pools of warm flicker drifting around the loop |
| Breathe 4-7-8 | Guided breathing; a fireball laps the ring during the hold |
| Aurora | Northern-lights curtains; four palettes, 10–300 % speed |
| Rainbow | The hue wheel around the perimeter, slowly rotating |
| Comets | Three comets with fading tails |
| Race | Two comets sprinting in opposite directions, up to ~8 laps a second |
| Horror | Fluorescent tubes malfunctioning over a dim pulsing background; both colors yours |
| Pomodoro | 25/5 cycles with a long break every fourth; unmissable phase changes |
| Metronome | 4/4 playhead with the beats on the corners; tempo from the page or HA |

Mode, brightness, colors, palette, speed and the reading light are
remembered across power cycles. The dinner timer is never restored on boot.

## Home Assistant

Point `secrets.h` at an MQTT broker and the lamp auto-discovers as one
device with a light, a mode select, a reading light, the timers and the
aurora settings. Entities, broker setup and example automations:
[docs/home-assistant.md](docs/home-assistant.md).

## Adding a light mode

Create `src/effects/MyEffect.h`:

```cpp
#pragma once
#include "Effect.h"

class MyEffect : public Effect {
public:
  const char* name() const override { return "My effect"; }
  void update(HD108& strip, uint32_t nowMs) override {
    // strip.setPixel(i, r16, g16, b16) / strip.setPixel8(i, r, g, b)
    // geo::frac(i), geo::xy(i), geo::angle(i) for position-aware effects
  }
  // Optional: begin() runs once when the mode is selected (default: clear);
  // frameMs() asks for a faster frame clock; repaint() should be a no-op
  // if update() redraws every pixel anyway (overlays rely on it).
};
```

Then one instance and one entry in `g_effects[]` in `src/main.cpp`. It
appears on the web page and in Home Assistant automatically. The active
mode is remembered by name, so the list order is free.

## The HD108 driver

`lib/HD108` is a self-contained ESP32 driver for HD108 strips: 16-bit color
per channel, per-pixel 5-bit current gain, configurable color order, ring
rotation and gamma helpers, with one preassembled wire buffer and a single
SPI transfer per frame. It has no dependency on the rest of this project
and can be dropped into any PlatformIO or Arduino ESP32 project. Protocol
details and the API: [lib/HD108/README.md](lib/HD108/README.md).

## Repo layout

```
platformio.ini                build target; OTA settings go in platformio_local.ini (gitignored)
include/config.h              pins, LED count, geometry, defaults — fit it to your lamp here
include/geometry.h            LED index -> position on the rectangle
include/secrets.h.example     WiFi, OTA and MQTT credentials (copy to secrets.h)
lib/HD108/                    the HD108 driver, packaged as a library
src/main.cpp                  boot, WiFi, OTA, modes, the overlays
src/ReadingZone.h             reading-light overlay
src/webui.*                   web page + JSON API
src/hamqtt.*                  Home Assistant MQTT discovery + entities
src/effects/                  one header per light mode
docs/home-assistant.md        HA entities, broker setup, example automations
```

## License

MIT, see [LICENSE](LICENSE).
