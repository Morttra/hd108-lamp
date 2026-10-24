# Home Assistant

The lamp speaks MQTT with Home Assistant auto-discovery (through the
[ArduinoHA](https://github.com/dawidchyrzynski/arduino-home-assistant)
library), so nothing is configured on the HA side. Once `include/secrets.h`
points at a broker, a device called **HD108 Lamp** appears with these
entities. HA derives entity IDs from the device name plus the entity name;
the IDs below assume the default names.

| Entity | What it does |
|---|---|
| `light.hd108_lamp_light` | on/off, brightness (= LED current gain), RGB color (switches to Solid color) |
| `select.hd108_lamp_mode` | light mode (Off, Warm white, Aurora, Candle, …) |
| `light.hd108_lamp_reading_light` | reading-light overlay: on/off and its own brightness, drawn over any mode or over the lamp being off |
| `number.hd108_lamp_reading_position` | where the reading light sits: LED index around the ring, 0 = first corner, then along the sides in the order of `SIDE0_COUNT..SIDE3_COUNT`. Setting it also switches the light on |
| `select.hd108_lamp_aurora_palette` | Aurora hue band (Northern, Rose, Dawn, Ember); picking one switches to Aurora |
| `number.hd108_lamp_aurora_speed` | Aurora drift tempo, 10–300 % of the default |
| `number.hd108_lamp_dinner_timer` | minutes; setting it starts the timer |
| `button.hd108_lamp_start_dinner_timer` | restart the last duration |
| `number.hd108_lamp_metronome_bpm` | metronome tempo, 30–240 |
| `button.hd108_lamp_doorbell_alert` | 8 s blue pulse over everything, then the lamp returns to what it was doing |

The lamp publishes its full state on every (re)connect, so HA never shows
the library's defaults. Leave `MQTT_HOST` empty in `secrets.h` to skip
MQTT entirely; the web page keeps working.

## Broker setup

1. Install the **Mosquitto broker** add-on (Settings → Add-ons → Add-on
   Store) and start it. Create a Home Assistant user for the lamp under
   Settings → People → Users; Mosquitto accepts HA users.
2. Settings → Devices & Services usually auto-detects Mosquitto; accept the
   **MQTT** integration.
3. Put the broker host (the HA machine's IP or `homeassistant.local`), the
   user and the password into `include/secrets.h` and reflash. The device
   shows up under the MQTT integration within seconds.

## Example automations

### Doorbell, or anything else, flashes the lamp

Any trigger works: a doorbell binary sensor, a lock keypad event, a camera
person detection.

```yaml
alias: Door alert on lamp
trigger:
  - platform: state
    entity_id: binary_sensor.front_door_doorbell
    to: "on"
action:
  - service: button.press
    target:
      entity_id: button.hd108_lamp_doorbell_alert
```

### Video meeting → calm red "on air" light

The macOS companion app exposes `binary_sensor.<mac>_camera_in_use`.

```yaml
alias: Meeting on-air light
trigger:
  - platform: state
    entity_id: binary_sensor.macbook_camera_in_use
    to: "on"
action:
  - service: light.turn_on
    target: {entity_id: light.hd108_lamp_light}
    data: {rgb_color: [255, 30, 10], brightness: 60}
mode: single
```

Add a companion automation on `to: "off"` that restores your usual scene.

### Reading light from a dashboard

The position number is a plain LED index, so a dashboard can aim the
reading light with a slider, or with a picture-elements card shaped like
the lamp whose tap targets call `number.set_value` with the index of that
spot. Turning the reading light on from `light.hd108_lamp_reading_light`
leaves the underlying mode running; switching it off hands the pixels back
without resetting the mode.

### Evening scene

A scene can set the mode and brightness together, for example
`select.hd108_lamp_mode` to Aurora with `select.hd108_lamp_aurora_palette`
Ember and `light.hd108_lamp_light` at a low brightness.
