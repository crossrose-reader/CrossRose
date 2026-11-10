#pragma once

#include <HalStorage.h>

#include <cstdint>

#include "../GfxRenderer/ImageToneProfile.h"

class Print;
class ZipFile;

class JpegToBmpConverter {
  // TwoBit is the tone-profiled cover format; the others are thumbnail formats.
  enum class Output : uint8_t { TwoBit, OneBit, Gray8 };
  // maxTargetHeight: see jpegFileToGray8BmpStreamWithSize().
  static bool jpegFileToBmpStreamInternal(FsFile& jpegFile, Print& bmpOut, int targetWidth, int targetHeight,
                                          Output output, bool crop = true, bool adaptiveContain = false,
                                          ImageToneProfile toneProfile = ImageToneProfile::Legacy,
                                          int maxTargetHeight = 0);

 public:
  static bool jpegFileToBmpStream(FsFile& jpegFile, Print& bmpOut, bool crop = true,
                                  ImageToneProfile toneProfile = ImageToneProfile::Legacy);
  // Convert with custom target size (for thumbnails)
  // Convert to 1-bit BMP (black and white only, no grays) for fast home screen rendering
  static bool jpegFileTo1BitBmpStreamWithSize(FsFile& jpegFile, Print& bmpOut, int targetMaxWidth, int targetMaxHeight,
                                              bool adaptiveContain = false);
  // Convert to an undithered 8-bit grayscale BMP, cropped to fill the target
  // size. Thumbnails in the display formats are derived from this source.
  // With maxTargetHeight above targetHeight, a cover narrower than the target
  // keeps its own aspect up to that height instead of losing its top and
  // bottom, so narrower crops can still be taken from the result.
  static bool jpegFileToGray8BmpStreamWithSize(FsFile& jpegFile, Print& bmpOut, int targetWidth, int targetHeight,
                                               int maxTargetHeight = 0);
};
