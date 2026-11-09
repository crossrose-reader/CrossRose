#pragma once

#include <string>

#include "AppCapabilities.h"

struct RecentBook;

// Thumbnails behind Carousel's cover artwork. Each book has a centre and a
// side thumbnail in black and white and, on builds with four-tone cover art,
// the same pair in four tones. They are all derived from one source image, so
// a cover is decoded once however many of them are missing.
namespace CarouselCoverArt {

// Build-time capability; see CROSSINK_APP_CAP_GRAY_COVER_ART. A constant, so
// images without it compile the four-tone paths away.
constexpr bool fourToneEnabled() { return CROSSINK_APP_CAP_GRAY_COVER_ART != 0; }

// Cache paths of the black-and-white thumbnails, as the theme resolves them.
std::string centerThumbPath(const RecentBook& book);
std::string sideThumbPath(const RecentBook& book);

// True when a thumbnail Carousel draws from is missing. The four-tone centre
// thumbnail is not counted: books whose cover has no grayscale source (GIF
// covers, XTC books) never get one and must not be retried on every visit.
bool needsThumbnails(const RecentBook& book);

// Writes the missing thumbnails of `book` from the BMP at `sourcePath`. Pass
// graySource = false when the source is the book's own one-bit centre
// thumbnail: the centre thumbnails are then left alone and only the side
// thumbnails are derived. Returns false, after logging, if any derivation
// failed; thumbnails already written stay.
bool deriveMissing(const RecentBook& book, const std::string& sourcePath, bool graySource);

}  // namespace CarouselCoverArt
