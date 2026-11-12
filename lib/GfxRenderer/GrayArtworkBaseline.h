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

// Turns a captured screen into a four-tone picture (see
// GfxRenderer::beginGrayArtworkCapture()). Captured gray pixels were left
// white in `frame` and marked in the overlay planes. A mark only stands where
// the pixel is still white, so black drawn over it afterwards stays black;
// the gray pixels that remain go black in `frame`, the base frame the gray
// pass lightens. Returns true when any pixel is gray.
inline bool resolveCapturedGrayArtwork(uint8_t* frame, uint8_t* lsb, uint8_t* msb, const size_t size) {
  uint8_t any = 0;
  for (size_t i = 0; i < size; ++i) {
    const auto gray = static_cast<uint8_t>((lsb[i] | msb[i]) & frame[i]);
    lsb[i] &= gray;
    msb[i] &= gray;
    frame[i] &= static_cast<uint8_t>(~gray);
    any |= gray;
  }
  return any != 0;
}

// FNV-1a over both encoded planes: tells whether the four-tone picture on the
// panel is the one about to be shown without keeping a third copy of it.
inline uint32_t grayArtworkPlanesHash(const uint8_t* lsb, const uint8_t* msb, const size_t size) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < size; ++i) hash = (hash ^ lsb[i]) * 16777619u;
  for (size_t i = 0; i < size; ++i) hash = (hash ^ msb[i]) * 16777619u;
  return hash;
}
