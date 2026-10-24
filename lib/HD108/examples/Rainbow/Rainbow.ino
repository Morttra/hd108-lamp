// Rainbow around a 60-pixel HD108 strip on ESP32 VSPI (data GPIO 23, clock
// GPIO 18). Wire ~33 ohm in series with data and clock near the first LED.
#include <ColorUtil.h>
#include <HD108.h>

HD108 strip(60, /*data*/ 23, /*clock*/ 18, /*SPI Hz*/ 10000000UL);

void setup() {
  strip.begin();
  strip.setColorOrder(ColorOrder::BGR);  // datasheet order; some batches are RGB
  strip.setGlobalGain(8);                // 8/31 of full current: plenty indoors
}

void loop() {
  const uint16_t h0 = (uint16_t)((millis() % 20000UL) * 65536UL / 20000UL);
  for (uint16_t i = 0; i < strip.size(); ++i) {
    uint16_t r, g, b;
    const uint16_t h = h0 + (uint16_t)((uint32_t)i * 65536 / strip.size());
    hsv2rgb16(h, 65535, 40000, r, g, b);
    strip.setPixel(i, r, g, b);
  }
  strip.show();
  delay(15);
}
