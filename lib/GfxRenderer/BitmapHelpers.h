#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "../Memory/Memory.h"
#include "ImageToneProfile.h"

struct BmpHeader;

// Helper functions
uint8_t quantize(int gray, int x, int y);
uint8_t quantizeSimple(int gray);
uint8_t quantize1bit(int gray, int x, int y);
int adjustPixel(int gray);

struct GrayPlanePixel {
  bool write;
  bool black;
};

// Levels: black, dark, light, white. drawPixel(true) clears a framebuffer bit.
constexpr GrayPlanePixel grayPlanePixel(uint8_t level, bool msb, bool absolute) {
  if (absolute) return {true, !(level == 3 || level == (msb ? 2 : 1))};
  return {msb ? (level == 1 || level == 2) : level == 1, false};
}

enum class BmpRowOrder { BottomUp, TopDown };

// Height of a crop-to-fill output `targetWidth` wide that keeps the source's
// own aspect, held between minHeight and maxHeight: a source wider than
// targetWidth:minHeight is still cropped to that aspect, and one narrower than
// targetWidth:maxHeight to that one. minHeight when maxHeight is not above it.
inline int aspectFillHeight(const int srcWidth, const int srcHeight, const int targetWidth, const int minHeight,
                            const int maxHeight) {
  if (maxHeight <= minHeight || srcWidth <= 0 || srcHeight <= 0 || targetWidth <= 0) return minHeight;
  const int64_t height = (static_cast<int64_t>(targetWidth) * srcHeight + srcWidth / 2) / srcWidth;
  if (height < minHeight) return minHeight;
  if (height > maxHeight) return maxHeight;
  return static_cast<int>(height);
}

// Populates a 1-bit BMP header in the provided memory.
void createBmpHeader(BmpHeader* bmpHeader, int width, int height, BmpRowOrder rowOrder);

// Decision boundaries and error-diffusion reference values for a four-tone image.
struct ImageTonePalette {
  uint8_t thresholds[3];
  uint8_t levels[4];

  uint8_t quantize(int gray) const {
    if (gray < thresholds[0]) return 0;
    if (gray < thresholds[1]) return 1;
    if (gray < thresholds[2]) return 2;
    return 3;
  }
};

inline constexpr ImageTonePalette legacyTonePalette{{30, 50, 140}, {15, 30, 80, 210}};
inline constexpr ImageTonePalette evenTonePalette{{43, 128, 213}, {0, 85, 170, 255}};
constexpr const ImageTonePalette& paletteFor(ImageToneProfile profile) {
  switch (profile) {
    case ImageToneProfile::Legacy:
      return legacyTonePalette;
    case ImageToneProfile::Even:
    case ImageToneProfile::PiccoCover:
      return evenTonePalette;
  }
  return legacyTonePalette;
}

// Error diffusion for cover thumbnails, using Stucki's weights (all / 42):
//             X   8   4
//     2   4   8   4   2
//     1   2   4   2   1
// It carries the whole quantization error forward, so mid-tones keep their
// shade; the Atkinson filter used before dropped a quarter of it and washed
// out low-contrast lettering.
//
// edgeBoost, in eighths, moves the decision threshold with the pixel itself
// (Eschbach-Knox threshold modulation). That sharpens edges and keeps
// near-black and near-white areas free of stray dots, and it needs no
// neighbouring rows, so converters can still feed one pixel at a time.
class ThumbnailDitherer {
 public:
  // Tuned on real covers at 296x468; stronger settings posterize logos.
  static constexpr int kOneBitEdgeBoost = 12;

  // levels is 2 (black/white) or 4 (the even four-tone palette).
  ThumbnailDitherer(int width, int levels, int edgeBoost = 0) : levels(levels == 4 ? 4 : 2), edgeBoost(edgeBoost) {
    if (width <= 0) return;
    const size_t candidateRowSize = static_cast<size_t>(width) + 4;
    if (candidateRowSize > SIZE_MAX / (3 * sizeof(int16_t))) return;
    rowSize = candidateRowSize;
    errorRows = makeUniqueNoThrow<int16_t[]>(rowSize * 3);
    if (!errorRows) return;
    errorRow0 = errorRows.get();
    errorRow1 = errorRow0 + rowSize;
    errorRow2 = errorRow1 + rowSize;
  }

  bool isValid() const { return errorRows != nullptr; }

  ThumbnailDitherer(const ThumbnailDitherer& other) = delete;
  ThumbnailDitherer& operator=(const ThumbnailDitherer& other) = delete;

  // Returns the tone index: 0 is black, levels - 1 is white.
  uint8_t processPixel(int gray, int x) {
    if (!isValid()) return quantizeTone(gray);

    // Rows hold the weighted error sum; at most 42 * 255, which fits int16_t.
    int adjusted = gray + errorRow0[x + 2] / kWeightSum;
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 255) adjusted = 255;

    const uint8_t tone = quantizeTone(adjusted + (edgeBoost * (gray - 128)) / 8);
    // The error is measured without the boost, so tone is conserved.
    const int error = adjusted - toneValue(tone);

    errorRow0[x + 3] += error * 8;
    errorRow0[x + 4] += error * 4;
    errorRow1[x] += error * 2;
    errorRow1[x + 1] += error * 4;
    errorRow1[x + 2] += error * 8;
    errorRow1[x + 3] += error * 4;
    errorRow1[x + 4] += error * 2;
    errorRow2[x] += error;
    errorRow2[x + 1] += error * 2;
    errorRow2[x + 2] += error * 4;
    errorRow2[x + 3] += error * 2;
    errorRow2[x + 4] += error;

    return tone;
  }

  void nextRow() {
    if (!isValid()) return;
    int16_t* temp = errorRow0;
    errorRow0 = errorRow1;
    errorRow1 = errorRow2;
    errorRow2 = temp;
    memset(errorRow2, 0, rowSize * sizeof(int16_t));
  }

 private:
  static constexpr int kWeightSum = 42;

  uint8_t quantizeTone(const int gray) const {
    if (levels == 2) return gray < 128 ? 0 : 1;
    return evenTonePalette.quantize(gray);
  }
  int toneValue(const uint8_t tone) const { return levels == 2 ? (tone ? 255 : 0) : evenTonePalette.levels[tone]; }

  int levels;
  int edgeBoost;
  size_t rowSize{0};
  std::unique_ptr<int16_t[]> errorRows;
  int16_t* errorRow0 = nullptr;
  int16_t* errorRow1 = nullptr;
  int16_t* errorRow2 = nullptr;
};

// Atkinson dithering - distributes only 6/8 (75%) of error for cleaner results
// Error distribution pattern:
//     X  1/8 1/8
// 1/8 1/8 1/8
//     1/8
// Less error buildup = fewer artifacts than Floyd-Steinberg
class AtkinsonDitherer {
 public:
  explicit AtkinsonDitherer(int width, ImageToneProfile toneProfile = ImageToneProfile::Legacy)
      : palette(paletteFor(toneProfile)) {
    if (width <= 0) return;
    const size_t candidateRowSize = static_cast<size_t>(width) + 4;
    if (candidateRowSize > SIZE_MAX / (3 * sizeof(int16_t))) return;
    rowSize = candidateRowSize;
    errorRows = makeUniqueNoThrow<int16_t[]>(rowSize * 3);
    if (!errorRows) return;
    errorRow0 = errorRows.get();
    errorRow1 = errorRow0 + rowSize;
    errorRow2 = errorRow1 + rowSize;
  }
  // **1. EXPLICITLY DELETE THE COPY CONSTRUCTOR**
  AtkinsonDitherer(const AtkinsonDitherer& other) = delete;

  // **2. EXPLICITLY DELETE THE COPY ASSIGNMENT OPERATOR**
  AtkinsonDitherer& operator=(const AtkinsonDitherer& other) = delete;

  bool isValid() const { return errorRows != nullptr; }

  uint8_t processPixel(int gray, int x) {
    // Add accumulated error
    int adjusted = gray + (isValid() ? errorRow0[x + 2] : 0);
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 255) adjusted = 255;

    const uint8_t quantized = palette.quantize(adjusted);
    if (!isValid()) return quantized;
    const int quantizedValue = palette.levels[quantized];

    // Calculate error (only distribute 6/8 = 75%)
    int error = (adjusted - quantizedValue) >> 3;  // error/8

    // Distribute 1/8 to each of 6 neighbors
    errorRow0[x + 3] += error;  // Right
    errorRow0[x + 4] += error;  // Right+1
    errorRow1[x + 1] += error;  // Bottom-left
    errorRow1[x + 2] += error;  // Bottom
    errorRow1[x + 3] += error;  // Bottom-right
    errorRow2[x + 2] += error;  // Two rows down

    return quantized;
  }

  void nextRow() {
    if (!isValid()) return;
    int16_t* temp = errorRow0;
    errorRow0 = errorRow1;
    errorRow1 = errorRow2;
    errorRow2 = temp;
    memset(errorRow2, 0, rowSize * sizeof(int16_t));
  }

  void reset() {
    if (!isValid()) return;
    memset(errorRow0, 0, rowSize * sizeof(int16_t));
    memset(errorRow1, 0, rowSize * sizeof(int16_t));
    memset(errorRow2, 0, rowSize * sizeof(int16_t));
  }

 private:
  const ImageTonePalette& palette;
  size_t rowSize{0};
  std::unique_ptr<int16_t[]> errorRows;
  int16_t* errorRow0 = nullptr;
  int16_t* errorRow1 = nullptr;
  int16_t* errorRow2 = nullptr;
};

// Floyd-Steinberg error diffusion dithering with serpentine scanning
// Serpentine scanning alternates direction each row to reduce "worm" artifacts
// Error distribution pattern (left-to-right):
//       X   7/16
// 3/16 5/16 1/16
// Error distribution pattern (right-to-left, mirrored):
// 1/16 5/16 3/16
//      7/16  X
class FloydSteinbergDitherer {
 public:
  explicit FloydSteinbergDitherer(int width, ImageToneProfile toneProfile = ImageToneProfile::Legacy)
      : palette(paletteFor(toneProfile)), rowCount(0) {
    if (width <= 0) return;
    const size_t candidateRowSize = static_cast<size_t>(width) + 2;
    if (candidateRowSize > SIZE_MAX / (2 * sizeof(int16_t))) return;
    rowSize = candidateRowSize;
    errorRows = makeUniqueNoThrow<int16_t[]>(rowSize * 2);
    if (!errorRows) return;
    errorCurRow = errorRows.get();
    errorNextRow = errorCurRow + rowSize;
  }

  bool isValid() const { return errorRows != nullptr; }

  // **1. EXPLICITLY DELETE THE COPY CONSTRUCTOR**
  FloydSteinbergDitherer(const FloydSteinbergDitherer& other) = delete;

  // **2. EXPLICITLY DELETE THE COPY ASSIGNMENT OPERATOR**
  FloydSteinbergDitherer& operator=(const FloydSteinbergDitherer& other) = delete;

  // Process a single pixel and return quantized 2-bit value
  // x is the logical x position (0 to width-1), direction handled internally
  uint8_t processPixel(int gray, int x) {
    // Add accumulated error to this pixel
    int adjusted = gray + (isValid() ? errorCurRow[x + 1] : 0);

    // Clamp to valid range
    if (adjusted < 0) adjusted = 0;
    if (adjusted > 255) adjusted = 255;

    const uint8_t quantized = palette.quantize(adjusted);
    if (!isValid()) return quantized;
    const int quantizedValue = palette.levels[quantized];

    // Calculate error
    int error = adjusted - quantizedValue;

    // Distribute error to neighbors (serpentine: direction-aware)
    if (!isReverseRow()) {
      // Left to right: standard distribution
      // Right: 7/16
      errorCurRow[x + 2] += (error * 7) >> 4;
      // Bottom-left: 3/16
      errorNextRow[x] += (error * 3) >> 4;
      // Bottom: 5/16
      errorNextRow[x + 1] += (error * 5) >> 4;
      // Bottom-right: 1/16
      errorNextRow[x + 2] += (error) >> 4;
    } else {
      // Right to left: mirrored distribution
      // Left: 7/16
      errorCurRow[x] += (error * 7) >> 4;
      // Bottom-right: 3/16
      errorNextRow[x + 2] += (error * 3) >> 4;
      // Bottom: 5/16
      errorNextRow[x + 1] += (error * 5) >> 4;
      // Bottom-left: 1/16
      errorNextRow[x] += (error) >> 4;
    }

    return quantized;
  }

  // Call at the end of each row to swap buffers
  void nextRow() {
    if (!isValid()) return;
    // Swap buffers
    int16_t* temp = errorCurRow;
    errorCurRow = errorNextRow;
    errorNextRow = temp;
    // Clear the next row buffer
    memset(errorNextRow, 0, rowSize * sizeof(int16_t));
    rowCount++;
  }

  // Check if current row should be processed in reverse
  bool isReverseRow() const { return (rowCount & 1) != 0; }

  // Reset for a new image or MCU block
  void reset() {
    if (!isValid()) return;
    memset(errorCurRow, 0, rowSize * sizeof(int16_t));
    memset(errorNextRow, 0, rowSize * sizeof(int16_t));
    rowCount = 0;
  }

 private:
  const ImageTonePalette& palette;
  int rowCount;
  size_t rowSize{0};
  std::unique_ptr<int16_t[]> errorRows;
  int16_t* errorCurRow = nullptr;
  int16_t* errorNextRow = nullptr;
};
