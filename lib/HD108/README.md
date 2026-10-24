# HD108 driver for ESP32

A small, dependency-free driver for **HD108** addressable RGB LEDs on the
ESP32, using hardware SPI. HD108 is an APA102/SK9822-style two-wire part
(data + clock) with two things the 8-bit strips lack: **16 bits per color
channel** (65 536 gray levels at ~28 kHz PWM) and a **5-bit constant-current
gain per pixel** (0–31 steps of ~17 mA). Deep dims stay smooth, and
brightness can be set by current instead of by throwing away PWM
resolution.

Written for a 680-LED lounge lamp; the lamp firmware that uses it lives in
the parent repo. The driver has no dependency on it and can be dropped into
any PlatformIO or Arduino ESP32 project.

## Wire format

```
start frame : 128 zero bits
per pixel   : 64 bits, MSB first
              1 | gain(5) | gain(5) | gain(5) | c0(16) | c1(16) | c2(16)
end frame   : >= 64 one bits, plus one extra clock edge per two pixels
```

The three gain fields set the constant current of the three outputs; the
16-bit fields are PWM gray levels. Each HD108 delays the regenerated clock
by half a period, which is why the end frame grows with the pixel count:
without the extra clocks the last pixels never latch.

The datasheet lists the color fields as B,G,R, but production batches
differ (the strips this was written for turned out to be R,G,B), so the
order is configurable.

## Usage

```cpp
#include <HD108.h>

HD108 strip(340, /*data*/ 23, /*clock*/ 18, /*SPI Hz*/ 10000000UL);

void setup() {
  strip.begin();
  strip.setColorOrder(ColorOrder::RGB);
  strip.setGlobalGain(10);               // 10/31 of full current
}

void loop() {
  strip.setPixel(0, 65535, 30000, 5000); // linear 16-bit values
  strip.setPixel8(1, 255, 147, 41);      // 8-bit sRGB-ish, gamma-expanded
  strip.show();
}
```

A second strip on the other bus:

```cpp
SPIClass hspi(HSPI);
HD108 up(340, /*data*/ 13, /*clock*/ 14, 10000000UL, hspi);
```

## API

| Call | What it does |
|---|---|
| `HD108(n, dataPin, clockPin, spiHz = 10 MHz, spi = SPI)` | n pixels on the given pins; `spi` picks the bus (VSPI by default) |
| `begin()` / `end()` | allocate the wire buffer and claim the bus; returns false if allocation fails |
| `setColorOrder(ColorOrder)` | BGR, RGB, GRB, BRG, RBG or GBR on the wire |
| `setOffset(int16_t)` | rotate the ring: logical pixel 0 maps to physical pixel `offset` (may be negative) |
| `setGlobalGain(0..31)` / `globalGain()` | current gain for every pixel — a flicker-free master brightness |
| `setPixelGain(i, 0..31)` | per-pixel current gain; `setGlobalGain()` resets all pixels |
| `setPixel(i, r, g, b)` | linear 16-bit values |
| `setPixel8(i, r, g, b)` | 8-bit values through a 2.2 gamma table |
| `getPixel(i, r, g, b)` | read back the 16-bit values |
| `fill(r, g, b)` / `clear()` | every pixel |
| `show()` | one blocking SPI transfer of the whole buffer |
| `size()` | pixel count |
| `HD108::gamma16(v)` | the 8-bit → 16-bit gamma expansion |

`ColorUtil.h` adds `hsv2rgb16(h, s, v, r, g, b)` (all 0..65535, output is
linear) and `lerp16(a, b, frac)`.

## Notes

- `show()` blocks for ~2.2 ms per 340 pixels at 10 MHz, so >60 fps leaves
  plenty of loop time for WiFi and the rest.
- HD108 accepts up to 32 MHz. 10 MHz is a safe default over real wiring at
  3.3 V logic into a 5 V strip. Put ~33 Ω in series with data and clock
  near the first LED; add a level shifter (74AHCT125 or similar) if the
  strip glitches at higher clocks.
- Values given to `setPixel()` are linear light. HSV hues from
  `hsv2rgb16()` are too, which reads differently from sRGB intuition: a
  little green mixed into red already looks yellow, and a little blue
  looks pink.
- ESP32 only: the driver relies on `SPIClass::begin(sck, miso, mosi, ss)`
  and `SPIClass::writeBytes()`. Porting means replacing those three calls
  in `HD108.cpp`.
- Memory: one preassembled buffer of 16 + 8·n + end-frame bytes (~2.8 KB
  for 340 pixels).

## License

MIT, same as the parent repo.
