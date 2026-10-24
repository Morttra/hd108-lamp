#include "HD108.h"

namespace {

constexpr size_t kStartBytes = 16;  // 128 zero bits
constexpr size_t kFrameBytes = 8;   // 64 bits per pixel

uint16_t s_gamma[256];
bool s_gammaReady = false;

void initGamma() {
  if (s_gammaReady) return;
  for (int i = 0; i < 256; ++i) {
    float lin = powf((float)i / 255.0f, 2.2f);
    s_gamma[i] = (uint16_t)lroundf(lin * 65535.0f);
  }
  s_gammaReady = true;
}

}  // namespace

HD108Strip::HD108Strip(uint16_t numLeds, int8_t dataPin, int8_t clockPin, uint32_t spiHz,
             SPIClass& spi)
    : n_(numLeds), dataPin_(dataPin), clockPin_(clockPin), hz_(spiHz),
      spi_(&spi) {}

HD108Strip::~HD108Strip() { end(); }

bool HD108Strip::begin() {
  if (begun_) return true;
  initGamma();
  // >= 64 one bits, plus one extra clock edge per two LEDs (each HD108 delays
  // the regenerated clock by half a period).
  endLen_ = 8 + ((size_t)n_ + 15) / 16;
  len_ = kStartBytes + (size_t)n_ * kFrameBytes + endLen_;
  buf_ = (uint8_t*)malloc(len_);
  if (buf_ == nullptr) return false;
  memset(buf_, 0x00, len_ - endLen_);
  memset(buf_ + len_ - endLen_, 0xFF, endLen_);
  for (uint16_t i = 0; i < n_; ++i) writeHeader(i, gain_);
  spi_->begin(clockPin_, /*miso=*/-1, dataPin_, /*ss=*/-1);
  begun_ = true;
  return true;
}

void HD108Strip::end() {
  if (!begun_) return;
  spi_->end();
  free(buf_);
  buf_ = nullptr;
  begun_ = false;
}

void HD108Strip::setColorOrder(ColorOrder order) {
  switch (order) {
    case ColorOrder::BGR: slotB_ = 0; slotG_ = 1; slotR_ = 2; break;
    case ColorOrder::RGB: slotR_ = 0; slotG_ = 1; slotB_ = 2; break;
    case ColorOrder::GRB: slotG_ = 0; slotR_ = 1; slotB_ = 2; break;
    case ColorOrder::BRG: slotB_ = 0; slotR_ = 1; slotG_ = 2; break;
    case ColorOrder::RBG: slotR_ = 0; slotB_ = 1; slotG_ = 2; break;
    case ColorOrder::GBR: slotG_ = 0; slotB_ = 1; slotR_ = 2; break;
  }
}

void HD108Strip::writeHeader(uint16_t phys, uint8_t gain) {
  const uint16_t g5 = gain & 0x1F;
  const uint16_t hdr = 0x8000 | (g5 << 10) | (g5 << 5) | g5;
  uint8_t* p = buf_ + kStartBytes + (size_t)phys * kFrameBytes;
  p[0] = hdr >> 8;
  p[1] = hdr & 0xFF;
}

void HD108Strip::setGlobalGain(uint8_t gain) {
  gain_ = gain > 31 ? 31 : gain;
  if (buf_ == nullptr) return;
  for (uint16_t i = 0; i < n_; ++i) writeHeader(i, gain_);
}

void HD108Strip::setPixelGain(uint16_t i, uint8_t gain) {
  if (i >= n_ || buf_ == nullptr) return;
  writeHeader(phys(i), gain > 31 ? 31 : gain);
}

void HD108Strip::setPixel(uint16_t i, uint16_t r, uint16_t g, uint16_t b) {
  if (i >= n_ || buf_ == nullptr) return;
  uint8_t* p = buf_ + kStartBytes + (size_t)phys(i) * kFrameBytes + 2;
  p[slotR_ * 2] = r >> 8;
  p[slotR_ * 2 + 1] = r & 0xFF;
  p[slotG_ * 2] = g >> 8;
  p[slotG_ * 2 + 1] = g & 0xFF;
  p[slotB_ * 2] = b >> 8;
  p[slotB_ * 2 + 1] = b & 0xFF;
}

void HD108Strip::setPixel8(uint16_t i, uint8_t r, uint8_t g, uint8_t b) {
  setPixel(i, gamma16(r), gamma16(g), gamma16(b));
}

void HD108Strip::getPixel(uint16_t i, uint16_t& r, uint16_t& g, uint16_t& b) const {
  if (i >= n_ || buf_ == nullptr) {
    r = g = b = 0;
    return;
  }
  const uint8_t* p = buf_ + kStartBytes + (size_t)phys(i) * kFrameBytes + 2;
  r = ((uint16_t)p[slotR_ * 2] << 8) | p[slotR_ * 2 + 1];
  g = ((uint16_t)p[slotG_ * 2] << 8) | p[slotG_ * 2 + 1];
  b = ((uint16_t)p[slotB_ * 2] << 8) | p[slotB_ * 2 + 1];
}

void HD108Strip::fill(uint16_t r, uint16_t g, uint16_t b) {
  for (uint16_t i = 0; i < n_; ++i) setPixel(i, r, g, b);
}

void HD108Strip::show() {
  if (!begun_) return;
  spi_->beginTransaction(SPISettings(hz_, MSBFIRST, SPI_MODE0));
  spi_->writeBytes(buf_, len_);
  spi_->endTransaction();
}

uint16_t HD108Strip::gamma16(uint8_t v) {
  initGamma();
  return s_gamma[v];
}
