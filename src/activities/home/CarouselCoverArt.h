#pragma once

#include <cstdint>
#include <string>

#include "AppCapabilities.h"

struct RecentBook;

// Thumbnails behind Carousel's cover artwork. Each book has a centre and a
// side thumbnail in black and white and, on builds with four-tone cover art,
// the same pair in four tones. They are all derived from one source image, so
// a cover is decoded once however many of them are missing.
namespace CarouselCoverArt {

// Build-time capability; see CROSSINK_APP_CAP_GRAY_COVER_ART. A constant, so
// Home compiles its calls into the four-tone paths away on other images.
constexpr bool fourToneEnabled() { return CROSSINK_APP_CAP_GRAY_COVER_ART != 0; }

// Cache paths of the black-and-white thumbnails, as the theme resolves them.
std::string centerThumbPath(const RecentBook& book);
std::string sideThumbPath(const RecentBook& book);

// True when a thumbnail Carousel draws from is missing. On four-tone builds
// the four-tone side thumbnail also marks the book's four-tone set complete:
// deriveMissing() writes it last, and only once the four-tone centre exists
// or the cover has no grayscale source. A book whose four-tone centre failed
// is therefore retried, while GIF covers and XTC books, which never get one,
// are not retried on every visit.
bool needsThumbnails(const RecentBook& book);

// What deriveMissing() derives from.
enum class Source : uint8_t {
  // The 8-bit cover source (Epub::generateThumbSourceBmp()): every thumbnail.
  Gray,
  // The book's one-bit centre thumbnail, as good as its cover gets (GIF
  // covers, XTC books): the side thumbnails, four-tone included.
  OneBitCover,
  // The one-bit centre standing in after the 8-bit source failed, so the book
  // still shows: only the black-and-white side. The four-tone thumbnails stay
  // missing, and needsThumbnails() retries them on the next visit.
  OneBitStandIn,
};

struct DeriveResult {
  bool complete = true;  // every thumbnail asked for exists now
  bool changed = false;  // at least one was written
};

// Writes the missing thumbnails of `book` that `source` can provide from the
// BMP at `sourcePath`. Failures are logged; thumbnails already written stay.
DeriveResult deriveMissing(const RecentBook& book, const std::string& sourcePath, Source source);

}  // namespace CarouselCoverArt
