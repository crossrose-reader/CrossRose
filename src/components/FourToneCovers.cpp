#include "FourToneCovers.h"

#include <GfxRenderer.h>
#include <ThumbnailFormat.h>

namespace FourToneCovers {

std::string variantPath(const std::string& oneBitPath) {
  return thumbnailVariantPath(oneBitPath, ThumbnailFormat::FourTone);
}

bool openThumb(const GfxRenderer& renderer, const std::string& oneBitPath, FsFile& file) {
  if (oneBitPath.empty()) return false;
  // Outside a capture a four-tone thumbnail would draw its grays black.
  if (enabled() && renderer.isCapturingGrayArtwork()) {
    const std::string fourTone = variantPath(oneBitPath);
    if (Storage.exists(fourTone.c_str()) && Storage.openFileForRead("HOME", fourTone, file)) return true;
  }
  return Storage.openFileForRead("HOME", oneBitPath, file);
}

}  // namespace FourToneCovers
