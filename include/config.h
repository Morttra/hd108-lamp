#pragma once

// ============================== LED strip ===================================

// Addressable pixels in the data stream. The up- and down-facing strips hang
// off the same data/clock pair, so this is the length of one strip.
#define LED_COUNT 340

// Hardware SPI (VSPI).
#define PIN_LED_DATA  23  // VSPI MOSI Red wire
#define PIN_LED_CLOCK 18  // VSPI SCLK Yellow wire

// HD108 accepts up to 32 MHz. 10 MHz is a robust default over real-world
// wiring at 3.3 V logic; raise it if the strip stays stable.
#define LED_SPI_HZ 10000000UL

// Wire order of the color fields. The datasheet claims B,G,R but this
// strip's batch is R,G,B — verified on the real strip on 2026-07-12
// (red/blue were swapped under BGR). Options: BGR, RGB, GRB, BRG, RBG, GBR.
#define LED_COLOR_ORDER ColorOrder::RGB

// Default global 5-bit current gain, 0..31 (31 = full 17 mA per channel).
// Full white at gain 31 draws roughly 35 A / 175 W across both strips —
// keep this modest unless your PSU and wiring are sized for it.
#define LED_DEFAULT_GAIN 10

// Target frame interval in ms (15 ms ~ 66 FPS; one full frame takes ~2.2 ms
// on the wire at 10 MHz, so there is plenty of headroom).
#define FRAME_MS 15

// ========================= Geometry (calibrate me) ==========================
// LED counts per side of the 2.0 m x 1.2 m rectangle, in data-line order,
// starting with the first LED after the controller. Count them once and fix
// these numbers — all position-aware effects depend on them.
// Must sum to LED_COUNT.
#define SIDE0_COUNT 98  // 2.0 m side
#define SIDE1_COUNT 72   // 1.2 m side
#define SIDE2_COUNT 98  // 2.0 m side
#define SIDE3_COUNT 72   // 1.2 m side (sums to LED_COUNT = 340)

#define RECT_W_M 2.0f
#define RECT_H_M 1.2f

// Rotates the whole ring so logical position 0 sits exactly on the first
// corner: physical LED = (logical + LED_OFFSET) mod LED_COUNT. Measured
// 2026-07-15: the corner is the strip's LAST LED, one before index 0.
#define LED_OFFSET -1

// ============================== Effects =====================================

// Doorbell/notification overlay duration (triggered from Home Assistant).
#define ALERT_MS 8000

// ============================== Network =====================================

#define HOSTNAME "hd108-lamp"
