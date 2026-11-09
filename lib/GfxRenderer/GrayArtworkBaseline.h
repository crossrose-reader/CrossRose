#pragma once

#include <cstddef>
#include <cstdint>

// Overlay grayscale leaves gray ink the controller still believes is black. A
// differential refresh only drives pixels whose colour changes, so those
// pixels would stay gray wherever the next frame wants black.
//
// Rewrites `shown`, the B/W frame the controller believes is on the panel, so
// that every gray pixel (a 1 bit in `grayMask`) claims the opposite colour of
// `next`. The following differential refresh then drives each of them to its
// new colour, and leaves every other pixel to the ordinary comparison.
// Framebuffer convention: a 1 bit is white.
inline void buildGrayArtworkBaseline(uint8_t* shown, const uint8_t* grayMask, const uint8_t* next, const size_t size) {
  for (size_t i = 0; i < size; ++i) {
    shown[i] = static_cast<uint8_t>((shown[i] & ~grayMask[i]) | (~next[i] & grayMask[i]));
  }
}
