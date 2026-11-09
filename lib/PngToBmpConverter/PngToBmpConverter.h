#pragma once

#include <HalStorage.h>

#include <cstdint>

#include "../GfxRenderer/ImageToneProfile.h"

class Print;

class PngToBmpConverter {
  // TwoBit is the tone-profiled cover format; the others are thumbnail formats.
  enum class Output : uint8_t { TwoBit, OneBit, Gray8 };
  static bool pngFileToBmpStreamInternal(FsFile& pngFile, Print& bmpOut, int targetWidth, int targetHeight,
                                         Output output, bool crop = true, bool adaptiveContain = false,
                                         ImageToneProfile toneProfile = ImageToneProfile::Legacy);

 public:
  static bool pngFileToBmpStream(FsFile& pngFile, Print& bmpOut, bool crop = true,
                                 ImageToneProfile toneProfile = ImageToneProfile::Legacy);
  static bool pngFileTo1BitBmpStreamWithSize(FsFile& pngFile, Print& bmpOut, int targetMaxWidth, int targetMaxHeight,
                                             bool adaptiveContain = false);
  // Convert to an undithered 8-bit grayscale BMP, cropped to fill the target
  // size. Thumbnails in the display formats are derived from this source.
  static bool pngFileToGray8BmpStreamWithSize(FsFile& pngFile, Print& bmpOut, int targetWidth, int targetHeight);
};
