# HD108 driver (lamp copy)

The core of the HD108 driver this firmware uses: `HD108Strip` in
`src/HD108.h` and the HSV/lerp helpers in `src/ColorUtil.h`. It is a
snapshot of the standalone **hd108-driver** library, which also carries the
NeoPixel-style and FastLED adapters, examples and full documentation. The
lamp needs none of those, so only the core is vendored here.

Quick reference:

| Call | What it does |
|---|---|
| `HD108Strip(n, dataPin, clockPin, spiHz = 10 MHz, spi = SPI)` | n pixels on the given pins |
| `begin()` | allocate the wire buffer and claim the SPI bus |
| `setColorOrder(ColorOrder)` / `setOffset(int16_t)` | wire order of the color fields; ring rotation |
| `setGlobalGain(0..31)` / `setPixelGain(i, 0..31)` | current gain for all pixels / one pixel |
| `setPixel(i, r, g, b)` / `setPixel8(i, r, g, b)` | linear 16-bit values / 8-bit through a 2.2 gamma |
| `getPixel`, `fill`, `clear`, `show`, `size` | as named |
| `HD108Strip::gamma16(v)` | the 8-bit → 16-bit gamma expansion |
