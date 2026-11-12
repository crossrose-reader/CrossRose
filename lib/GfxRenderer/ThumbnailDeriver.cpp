#include "ThumbnailDeriver.h"

#include <Logging.h>
#include <Memory.h>
#include <Print.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "Bitmap.h"
#include "BitmapHelpers.h"
#include "BufferedPrint.h"

namespace {

// Four-tone thumbnails are sharpened with a 3x3 unsharp mask before dithering.
// One-bit thumbnails rely on the ditherer's edge boost instead: the mask adds
// stray dots to their dark areas.
constexpr int kFourToneSharpenPercent = 100;
constexpr int kMaxWidth = 2048;
constexpr int kMaxHeight = 3072;
// Rows move to and from the SD card in blocks of about this size. A read
// costs about 1.4 ms on the Picco whatever its size up to a few sectors, so
// one call per 296-byte row spent 0.6 s of each 0.8 s derivation reading.
constexpr size_t kIoBlockBytes = 4096;

bool writeAll(Print& out, const uint8_t* data, const size_t size) { return out.write(data, size) == size; }

void putLe16(uint8_t* at, const uint16_t value) {
  at[0] = static_cast<uint8_t>(value);
  at[1] = static_cast<uint8_t>(value >> 8);
}

void putLe32(uint8_t* at, const uint32_t value) {
  at[0] = static_cast<uint8_t>(value);
  at[1] = static_cast<uint8_t>(value >> 8);
  at[2] = static_cast<uint8_t>(value >> 16);
  at[3] = static_cast<uint8_t>(value >> 24);
}

bool writeHeader(Print& out, const int width, const int height, const bool topDown, const int bitsPerPixel,
                 const uint32_t rowBytes) {
  const uint32_t colors = 1u << bitsPerPixel;
  const uint32_t pixelOffset = 54 + colors * 4;
  const uint32_t imageSize = rowBytes * static_cast<uint32_t>(height);
  uint8_t header[54 + 4 * 4] = {};
  header[0] = 'B';
  header[1] = 'M';
  putLe32(header + 2, pixelOffset + imageSize);
  putLe32(header + 10, pixelOffset);
  putLe32(header + 14, 40);
  putLe32(header + 18, static_cast<uint32_t>(width));
  putLe32(header + 22, static_cast<uint32_t>(topDown ? -height : height));
  putLe16(header + 26, 1);
  putLe16(header + 28, static_cast<uint16_t>(bitsPerPixel));
  putLe32(header + 34, imageSize);
  putLe32(header + 38, 2835);
  putLe32(header + 42, 2835);
  putLe32(header + 46, colors);
  putLe32(header + 50, colors);
  // Palette index is the tone index: 0 is black, the last entry white.
  for (uint32_t i = 0; i < colors; ++i) {
    const auto gray = static_cast<uint8_t>(i * 255 / (colors - 1));
    uint8_t* entry = header + 54 + i * 4;
    entry[0] = entry[1] = entry[2] = gray;
  }
  return writeAll(out, header, pixelOffset);
}

// Sharpens row `current` in place of `out`, clamping the vertical neighbours
// at the image edges (the caller passes the same row twice there).
void sharpenRow(const uint8_t* above, const uint8_t* current, const uint8_t* below, uint8_t* out, const int width) {
  for (int x = 0; x < width; ++x) {
    const int left = x > 0 ? x - 1 : 0;
    const int right = x + 1 < width ? x + 1 : width - 1;
    const int sum = above[left] + above[x] + above[right] + current[left] + current[x] + current[right] + below[left] +
                    below[x] + below[right];
    const int value = current[x] + (current[x] * 9 - sum) * kFourToneSharpenPercent / 900;
    out[x] = static_cast<uint8_t>(std::clamp(value, 0, 255));
  }
}

}  // namespace

bool deriveThumbnailBmp(HalFile& source, Print& destination, const ThumbnailSpec& spec) {
  // The size limits match the BMP reader's, which keeps the resampler's
  // products inside 32 bits.
  if (spec.width <= 0 || spec.height <= 0 || spec.width > kMaxWidth || spec.height > kMaxHeight ||
      spec.format == ThumbnailFormat::Gray8) {
    LOG_ERR("THB", "Unsupported thumbnail request: %dx%d format %d", spec.width, spec.height,
            static_cast<int>(spec.format));
    return false;
  }

  Bitmap bitmap(source);
  const BmpReaderError headerError = bitmap.parseHeaders();
  if (headerError != BmpReaderError::Ok) {
    LOG_ERR("THB", "Thumbnail source is not a readable BMP: %s", Bitmap::errorToString(headerError));
    return false;
  }
  const int srcWidth = bitmap.getWidth();
  const int srcHeight = bitmap.getHeight();

  // Centred crop with the requested aspect, in whole source pixels.
  const int64_t aspectW = spec.cropAspectWidth > 0 && spec.cropAspectHeight > 0 ? spec.cropAspectWidth : spec.width;
  const int64_t aspectH = spec.cropAspectWidth > 0 && spec.cropAspectHeight > 0 ? spec.cropAspectHeight : spec.height;
  const int cropWidth =
      static_cast<int>(std::clamp<int64_t>((srcHeight * aspectW + aspectH / 2) / aspectH, 1, srcWidth));
  const int cropHeight =
      static_cast<int>(std::clamp<int64_t>((srcWidth * aspectH + aspectW / 2) / aspectW, 1, srcHeight));
  const int cropX = (srcWidth - cropWidth) / 2;
  const int cropY = (srcHeight - cropHeight) / 2;

  const bool fourTone = spec.format == ThumbnailFormat::FourTone;
  const int bitsPerPixel = thumbnailBitsPerPixel(spec.format);
  const uint32_t outRowBytes = (static_cast<uint32_t>(spec.width) * bitsPerPixel + 31) / 32 * 4;

  // One scratch block for the whole conversion: a block of file rows, the
  // luminance of the row in use, three resampled rows (the sharpening window),
  // the sharpened row and the packed output row. With the 4 KB output buffer,
  // about 10 KB for Carousel's thumbnails, freed on return; sizes follow the
  // image, so the stack cannot hold them.
  const size_t fileRowBytes = static_cast<size_t>(bitmap.getRowBytes());
  const int readBlockRows = static_cast<int>(std::max<size_t>(1, kIoBlockBytes / fileRowBytes));
  const size_t scratchBytes =
      fileRowBytes * readBlockRows + static_cast<size_t>(srcWidth) + static_cast<size_t>(spec.width) * 4 + outRowBytes;
  auto scratch = makeUniqueNoThrow<uint8_t[]>(scratchBytes);
  auto sums = makeUniqueNoThrow<uint32_t[]>(spec.width);
  ThumbnailDitherer ditherer(spec.width, fourTone ? 4 : 2, fourTone ? 0 : ThumbnailDitherer::kOneBitEdgeBoost);
  if (!scratch || !sums || !ditherer.isValid()) {
    LOG_ERR("THB", "OOM deriving %dx%d thumbnail (%u scratch bytes)", spec.width, spec.height,
            static_cast<unsigned>(scratchBytes));
    return false;
  }
  uint8_t* readBlock = scratch.get();
  uint8_t* luminance = readBlock + fileRowBytes * readBlockRows;
  uint8_t* grayRows[3] = {luminance + srcWidth, luminance + srcWidth + spec.width,
                          luminance + srcWidth + spec.width * 2};
  uint8_t* sharpened = luminance + srcWidth + spec.width * 3;
  uint8_t* packed = sharpened + spec.width;
  BufferedPrint out(destination, kIoBlockBytes);

  if (!writeHeader(out, spec.width, spec.height, bitmap.isTopDown(), bitsPerPixel, outRowBytes)) {
    LOG_ERR("THB", "Short write of thumbnail header");
    return false;
  }

  // Rows are consumed in file order and each is read once; the crop is
  // centred, so the file's row order does not change which rows are used.
  int rowsRead = 0;   // file rows consumed; `luminance` holds row rowsRead - 1
  int blockRows = 0;  // rows in readBlock
  int blockNext = 0;  // the next of them to consume
  const auto readRow = [&](const int fileY) {
    while (rowsRead <= fileY) {
      if (blockNext == blockRows) {
        blockRows = bitmap.readRawRows(readBlock, readBlockRows);
        blockNext = 0;
        if (blockRows <= 0) {
          LOG_ERR("THB", "Thumbnail source row %d unreadable", rowsRead);
          return false;
        }
      }
      const uint8_t* raw = readBlock + fileRowBytes * blockNext++;
      // Rows above the crop are passed over undecoded.
      if (++rowsRead == fileY + 1) bitmap.rowLuminance(raw, luminance);
    }
    return true;
  };

  // Area-average one output row. Source pixel sx covers [sx*W, (sx+1)*W) and
  // output pixel x covers [x*cropWidth, (x+1)*cropWidth) on a shared grid, so
  // whole-number overlaps weight partial pixels exactly. A 1:1 crop passes
  // through untouched. The largest total is 255 * cropWidth * cropHeight,
  // inside uint32_t for every image the BMP reader accepts.
  const auto resampleRow = [&](const int outY, uint8_t* gray) {
    std::fill_n(sums.get(), spec.width, 0u);
    const int top = outY * cropHeight;
    const int bottom = top + cropHeight;
    const int firstRow = top / spec.height;
    const int endRow = (bottom + spec.height - 1) / spec.height;
    for (int sy = firstRow; sy < endRow; ++sy) {
      if (!readRow(cropY + sy)) return false;
      const auto yWeight =
          static_cast<uint32_t>(std::min(bottom, (sy + 1) * spec.height) - std::max(top, sy * spec.height));
      for (int x = 0; x < spec.width; ++x) {
        const int left = x * cropWidth;
        const int right = left + cropWidth;
        const int firstColumn = left / spec.width;
        const int endColumn = (right + spec.width - 1) / spec.width;
        uint32_t rowSum = 0;
        for (int sx = firstColumn; sx < endColumn; ++sx) {
          const auto xWeight =
              static_cast<uint32_t>(std::min(right, (sx + 1) * spec.width) - std::max(left, sx * spec.width));
          rowSum += static_cast<uint32_t>(luminance[cropX + sx]) * xWeight;
        }
        sums[x] += rowSum * yWeight;
      }
    }
    const uint32_t area = static_cast<uint32_t>(cropWidth) * static_cast<uint32_t>(cropHeight);
    for (int x = 0; x < spec.width; ++x) gray[x] = static_cast<uint8_t>((sums[x] + area / 2) / area);
    return true;
  };

  const auto writeRow = [&](const uint8_t* gray) {
    memset(packed, 0, outRowBytes);
    for (int x = 0; x < spec.width; ++x) {
      const uint8_t tone = ditherer.processPixel(gray[x], x);
      if (fourTone) {
        packed[x / 4] |= static_cast<uint8_t>(tone << (6 - (x % 4) * 2));
      } else {
        packed[x / 8] |= static_cast<uint8_t>(tone << (7 - (x % 8)));
      }
    }
    ditherer.nextRow();
    return writeAll(out, packed, outRowBytes);
  };

  bool ok = true;
  if (!fourTone) {
    for (int y = 0; ok && y < spec.height; ++y) ok = resampleRow(y, grayRows[0]) && writeRow(grayRows[0]);
  } else {
    // The sharpening window needs the row after the one being written, so
    // output trails the resampler by one row.
    uint8_t* above = grayRows[0];
    uint8_t* current = grayRows[1];
    uint8_t* below = grayRows[2];
    ok = resampleRow(0, current);
    for (int y = 0; ok && y < spec.height; ++y) {
      const bool last = y + 1 == spec.height;
      if (!last) ok = resampleRow(y + 1, below);
      if (!ok) break;
      sharpenRow(y == 0 ? current : above, current, last ? current : below, sharpened, spec.width);
      ok = writeRow(sharpened);
      uint8_t* recycled = above;
      above = current;
      current = below;
      below = recycled;
    }
  }
  ok = out.flushBuffer() && ok;
  if (!ok) LOG_ERR("THB", "Failed to derive %dx%d thumbnail", spec.width, spec.height);
  return ok;
}
