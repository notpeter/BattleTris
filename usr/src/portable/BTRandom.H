#ifndef BT_RANDOM_H
#define BT_RANDOM_H

#include <cstdint>

// Explicit 32-bit arithmetic makes portable sessions reproducible across native
// and WASM builds. Each board owns its stream; AI activity cannot advance yours.
// This preserves the selection weights, not historical libc random sequences.
class BTRandom {
  uint32_t state_ = 1;
public:
  void seed(uint32_t seed) { state_ = seed ? seed : 1; }
  uint32_t state() const { return state_; }
  int integer() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return static_cast<int>(state_ >> 1);
  }
  double unit() { return integer() / 2147483648.0; }
};
#endif
