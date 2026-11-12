#pragma once

#include <HalStorage.h>

#include <string>

#include "AppCapabilities.h"

class GfxRenderer;

// Four-tone cover thumbnails on screens drawn in one black-and-white pass:
// every Home theme except Carousel (which composes its own four-tone frames,
// see CarouselCoverArt) and the Library grid. The screen starts a capture
// with GfxRenderer::beginGrayArtworkCapture(), draws as usual, and shows the
// result with displayCapturedGrayArtwork(); covers drawn through openThumb()
// come out in four tones. Each one-bit thumbnail has a four-tone variant,
// written next to it by Epub::generateFourToneThumbBmp().
namespace FourToneCovers {

// Build-time capability; see CROSSINK_APP_CAP_GRAY_COVER_ART. A constant, so
// callers compile the four-tone paths away on other images.
constexpr bool enabled() { return CROSSINK_APP_CAP_GRAY_COVER_ART != 0; }

// The four-tone variant of the one-bit thumbnail at `oneBitPath`.
std::string variantPath(const std::string& oneBitPath);

// Opens the thumbnail to draw for the one-bit thumbnail at `oneBitPath`: its
// four-tone variant while `renderer` is capturing four-tone artwork and the
// variant exists, else `oneBitPath` itself. Both have the same size and
// framing, so layout code can read either one's dimensions.
bool openThumb(const GfxRenderer& renderer, const std::string& oneBitPath, FsFile& file);

}  // namespace FourToneCovers
