#pragma once

#include <HalStorage.h>

#include "ThumbnailFormat.h"

class Print;

struct ThumbnailSpec {
  // Size of the thumbnail written.
  int width = 0;
  int height = 0;
  // Aspect of the centred crop taken from the source. Zero keeps the output's
  // own aspect. A different aspect squeezes the crop into the output, which is
  // how Carousel's side covers are foreshortened before they are dithered.
  int cropAspectWidth = 0;
  int cropAspectHeight = 0;
  // OneBit or FourTone. Gray8 is a source format, not a derived one.
  ThumbnailFormat format = ThumbnailFormat::OneBit;
};

// Resamples a cover BMP of any depth the BMP reader accepts into a dithered
// thumbnail BMP. Dithering happens after the resample, at the size the
// thumbnail is drawn, so the pattern survives intact. Rows are written in the
// source's own order. Returns false, after logging, on bad input, a short
// write or an allocation failure; the caller removes the partial output.
bool deriveThumbnailBmp(HalFile& source, Print& out, const ThumbnailSpec& spec);
