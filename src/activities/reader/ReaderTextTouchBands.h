#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class GfxRenderer;
class Page;

// Screen areas of the rendered reader page that hold text, padded at least as generously as word
// selection's own hit test (WordSelectNavigator::selectWordAtPoint). A touch-and-hold outside
// every band cannot select a word, so the reader skips opening word selection for it. Consecutive
// lines merge into one band, so a page needs a handful of bands rather than one per line.
//
// When the geometry is unknown (nothing rendered yet, a failed render, or more bands than fit),
// mayContainWord() answers true and callers keep the full word-selection path. A false "maybe" only
// costs the old behaviour; a false "no" would block a real lookup, so every bound errs wide.
//
// Fixed-size storage (MAX_BANDS * 8 bytes) lives in the reader activity: no heap use per render.
class ReaderTextTouchBands {
 public:
  static constexpr size_t MAX_BANDS = 24;

  void clear() {
    count_ = 0;
    known_ = false;
  }

  // Rebuilds the bands from the page that was just rendered with these margins and font.
  void build(const Page& page, const GfxRenderer& renderer, int fontId, int marginTop, int marginLeft);

  // Starts an empty, known page (for build() and the simulator smoke test).
  void beginPage() {
    count_ = 0;
    known_ = true;
  }

  // Adds one padded text-line rectangle, merging it into the previous band when they overlap
  // vertically. Overflow marks the page unknown rather than dropping text.
  void addLine(int left, int top, int right, int bottom);

  bool mayContainWord(int x, int y) const;

 private:
  struct Band {
    int16_t left = 0;
    int16_t top = 0;
    int16_t right = 0;
    int16_t bottom = 0;
  };

  std::array<Band, MAX_BANDS> bands_{};
  uint8_t count_ = 0;
  bool known_ = false;
};
