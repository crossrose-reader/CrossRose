#pragma once

#include <cstdint>
#include <string>

// Pixel formats of cached cover thumbnails.
//   OneBit   - black and white, error diffused. Every panel can show it.
//   FourTone - 2 bpp holding the four tones of an overlay grayscale pass.
//   Gray8    - undithered source, kept only while the other formats are
//              derived from it.
enum class ThumbnailFormat : uint8_t { OneBit, FourTone, Gray8 };

constexpr const char* thumbnailFormatSuffix(const ThumbnailFormat format) {
  switch (format) {
    case ThumbnailFormat::OneBit:
      return "";
    case ThumbnailFormat::FourTone:
      return "_g4";
    case ThumbnailFormat::Gray8:
      return "_g8";
  }
  return "";
}

constexpr int thumbnailBitsPerPixel(const ThumbnailFormat format) {
  switch (format) {
    case ThumbnailFormat::OneBit:
      return 1;
    case ThumbnailFormat::FourTone:
      return 2;
    case ThumbnailFormat::Gray8:
      return 8;
  }
  return 1;
}

// Names the same thumbnail in another format: the suffix goes before the
// extension, so "thumb_296x468_v3.bmp" becomes "thumb_296x468_v3_g4.bmp".
// OneBit keeps the path, which is the name every existing caller uses.
inline std::string thumbnailVariantPath(const std::string& path, const ThumbnailFormat format) {
  if (format == ThumbnailFormat::OneBit || path.empty()) return path;
  const size_t dot = path.rfind('.');
  const size_t slash = path.rfind('/');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return path + thumbnailFormatSuffix(format);
  }
  std::string variant = path;
  variant.insert(dot, thumbnailFormatSuffix(format));
  return variant;
}
