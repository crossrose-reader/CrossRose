#include <BitmapHelpers.h>
#include <BufferedPrint.h>
#include <ThumbnailDeriver.h>
#include <ThumbnailFormat.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace {

struct Sink : Print {
  std::vector<uint8_t> bytes;
  size_t limit = SIZE_MAX;
  size_t write(uint8_t value) override {
    if (bytes.size() >= limit) return 0;
    bytes.push_back(value);
    return 1;
  }
};

uint32_t le32(const std::vector<uint8_t>& data, const size_t offset) {
  return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) |
         (static_cast<uint32_t>(data[offset + 3]) << 24);
}

void putLe32(std::vector<uint8_t>& data, const size_t offset, const uint32_t value) {
  for (int i = 0; i < 4; ++i) data[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}

// 8-bit grayscale BMP with an identity palette; gray(x, y) uses image rows, top first.
std::vector<uint8_t> grayBmp(const int width, const int height, const bool topDown,
                             const std::function<uint8_t(int, int)>& gray) {
  const int rowBytes = (width + 3) / 4 * 4;
  constexpr size_t kPixelOffset = 54 + 256 * 4;
  std::vector<uint8_t> data(kPixelOffset + static_cast<size_t>(rowBytes) * height, 0);
  data[0] = 'B';
  data[1] = 'M';
  putLe32(data, 2, static_cast<uint32_t>(data.size()));
  putLe32(data, 10, kPixelOffset);
  putLe32(data, 14, 40);
  putLe32(data, 18, width);
  putLe32(data, 22, static_cast<uint32_t>(topDown ? -height : height));
  data[26] = 1;
  data[28] = 8;
  putLe32(data, 46, 256);
  for (int i = 0; i < 256; ++i) {
    data[54 + i * 4] = data[54 + i * 4 + 1] = data[54 + i * 4 + 2] = static_cast<uint8_t>(i);
  }
  for (int y = 0; y < height; ++y) {
    const int fileRow = topDown ? y : height - 1 - y;
    for (int x = 0; x < width; ++x) data[kPixelOffset + static_cast<size_t>(fileRow) * rowBytes + x] = gray(x, y);
  }
  return data;
}

// Tone index of pixel (x, y), image rows top first, in a derived thumbnail.
int toneAt(const std::vector<uint8_t>& bmp, const int x, const int y) {
  const int width = static_cast<int>(le32(bmp, 18));
  const auto rawHeight = static_cast<int32_t>(le32(bmp, 22));
  const int height = rawHeight < 0 ? -rawHeight : rawHeight;
  const int bits = bmp[28];
  const int rowBytes = (width * bits + 31) / 32 * 4;
  const int fileRow = rawHeight < 0 ? y : height - 1 - y;
  const uint8_t* row = bmp.data() + le32(bmp, 10) + static_cast<size_t>(fileRow) * rowBytes;
  if (bits == 1) return (row[x / 8] >> (7 - x % 8)) & 1;
  return (row[x / 4] >> (6 - (x % 4) * 2)) & 3;
}

double whiteShare(ThumbnailDitherer& ditherer, const int gray, const int width, const int rows) {
  int white = 0;
  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < width; ++x) white += ditherer.processPixel(gray, x);
    ditherer.nextRow();
  }
  return static_cast<double>(white) / (width * rows);
}

}  // namespace

TEST(ThumbnailDitherer, KeepsTheShadeOfFlatAreas) {
  // Full error diffusion: the share of white pixels follows the input gray.
  for (const int gray : {32, 64, 128, 192, 224}) {
    ThumbnailDitherer ditherer(96, 2);
    EXPECT_NEAR(whiteShare(ditherer, gray, 96, 96), gray / 255.0, 0.03) << "gray " << gray;
  }
}

TEST(ThumbnailDitherer, EdgeBoostKeepsExtremesClean) {
  ThumbnailDitherer dark(64, 2, ThumbnailDitherer::kOneBitEdgeBoost);
  EXPECT_EQ(whiteShare(dark, 14, 64, 64), 0.0);
  ThumbnailDitherer light(64, 2, ThumbnailDitherer::kOneBitEdgeBoost);
  EXPECT_EQ(whiteShare(light, 241, 64, 64), 1.0);
}

TEST(ThumbnailDitherer, FourToneLeavesNativeLevelsSolid) {
  for (uint8_t tone = 0; tone < 4; ++tone) {
    ThumbnailDitherer ditherer(16, 4);
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 16; ++x) ASSERT_EQ(ditherer.processPixel(tone * 85, x), tone);
      ditherer.nextRow();
    }
  }
}

TEST(ThumbnailFormat, VariantPathsInsertTheSuffixBeforeTheExtension) {
  EXPECT_EQ(thumbnailVariantPath("/c/thumb_296x468_v3.bmp", ThumbnailFormat::OneBit), "/c/thumb_296x468_v3.bmp");
  EXPECT_EQ(thumbnailVariantPath("/c/thumb_296x468_v3.bmp", ThumbnailFormat::FourTone), "/c/thumb_296x468_v3_g4.bmp");
  EXPECT_EQ(thumbnailVariantPath("/c.d/thumb", ThumbnailFormat::Gray8), "/c.d/thumb_g8");
  EXPECT_EQ(thumbnailVariantPath("", ThumbnailFormat::FourTone), "");
}

TEST(ThumbnailDeriver, FourToneCopyOfNativeLevelsIsExactInEitherRowOrder) {
  constexpr int width = 20, height = 12;
  const auto tone = [](const int x, const int y) { return (x / 5 + y / 3) % 4; };
  for (const bool topDown : {true, false}) {
    HalFile source(
        grayBmp(width, height, topDown, [&](int x, int y) { return static_cast<uint8_t>(tone(x, y) * 85); }));
    // Flat 5x3 blocks: sharpening would ring at their edges, so compare interiors.
    Sink out;
    ASSERT_TRUE(deriveThumbnailBmp(source, out, {width, height, 0, 0, ThumbnailFormat::FourTone}));
    ASSERT_EQ(out.bytes.size(), le32(out.bytes, 2));
    EXPECT_EQ(le32(out.bytes, 18), static_cast<uint32_t>(width));
    EXPECT_EQ(static_cast<int32_t>(le32(out.bytes, 22)), topDown ? -height : height);
    EXPECT_EQ(out.bytes[28], 2);
    for (int y = 1; y < height; y += 3) {
      for (int x = 2; x < width; x += 5) EXPECT_EQ(toneAt(out.bytes, x, y), tone(x, y)) << x << "," << y;
    }
  }
}

TEST(ThumbnailDeriver, SqueezesTheCentredCropBeforeDithering) {
  // 40x20 source: white margins of 10 px, then a black left half and white right half.
  HalFile source(grayBmp(40, 20, true, [](int x, int) { return static_cast<uint8_t>(x >= 10 && x < 20 ? 0 : 255); }));
  Sink out;
  // Crop the centred 20x20 square, squeezed to 5 columns.
  ASSERT_TRUE(deriveThumbnailBmp(source, out, {5, 20, 1, 1, ThumbnailFormat::OneBit}));
  EXPECT_EQ(out.bytes[28], 1);
  for (int y = 0; y < 20; ++y) {
    EXPECT_EQ(toneAt(out.bytes, 0, y), 0);
    EXPECT_EQ(toneAt(out.bytes, 1, y), 0);
    EXPECT_EQ(toneAt(out.bytes, 3, y), 1);
    EXPECT_EQ(toneAt(out.bytes, 4, y), 1);
  }
}

TEST(ThumbnailDeriver, AveragesWholeSourcePixelsWhenShrinking) {
  // A 1 px checkerboard averages to mid-gray: half the output pixels are white.
  HalFile source(grayBmp(64, 64, true, [](int x, int y) { return static_cast<uint8_t>((x + y) % 2 ? 255 : 0); }));
  Sink out;
  ASSERT_TRUE(deriveThumbnailBmp(source, out, {32, 32, 0, 0, ThumbnailFormat::OneBit}));
  int white = 0;
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) white += toneAt(out.bytes, x, y);
  }
  EXPECT_NEAR(white, 512, 40);
}

TEST(ThumbnailDeriver, RejectsBadRequestsAndShortWrites) {
  const auto bmp = grayBmp(8, 8, true, [](int, int) { return uint8_t{128}; });
  {
    HalFile source(bmp);
    Sink out;
    EXPECT_FALSE(deriveThumbnailBmp(source, out, {8, 8, 0, 0, ThumbnailFormat::Gray8}));
    EXPECT_FALSE(deriveThumbnailBmp(source, out, {0, 8, 0, 0, ThumbnailFormat::OneBit}));
    EXPECT_TRUE(out.bytes.empty());
  }
  {
    HalFile source(std::vector<uint8_t>(64, 0));
    Sink out;
    EXPECT_FALSE(deriveThumbnailBmp(source, out, {8, 8, 0, 0, ThumbnailFormat::OneBit}));
  }
  Sink whole;
  {
    HalFile source(bmp);
    ASSERT_TRUE(deriveThumbnailBmp(source, whole, {8, 8, 0, 0, ThumbnailFormat::OneBit}));
  }
  for (size_t limit = 0; limit < whole.bytes.size(); limit += 7) {
    HalFile source(bmp);
    Sink out;
    out.limit = limit;
    EXPECT_FALSE(deriveThumbnailBmp(source, out, {8, 8, 0, 0, ThumbnailFormat::OneBit})) << limit;
  }
}

TEST(ThumbnailSource, AspectFillHeightKeepsTheCoverAspectWithinRange) {
  // Wider than 296:468: cropped to the centre thumbnail's aspect.
  EXPECT_EQ(aspectFillHeight(600, 800, 296, 468, 578), 468);
  // Between the centre and side framings: the cover's own aspect, uncropped.
  EXPECT_EQ(aspectFillHeight(600, 1000, 296, 468, 578), 493);
  // Narrower than 296:578: cropped to that aspect.
  EXPECT_EQ(aspectFillHeight(500, 1000, 296, 468, 578), 578);
  // Without a larger maximum it is the plain crop-to-fill height.
  EXPECT_EQ(aspectFillHeight(500, 1000, 296, 468, 0), 468);
  EXPECT_EQ(aspectFillHeight(0, 1000, 296, 468, 578), 468);
}

TEST(ThumbnailDeriver, ReadsAndWritesAcrossRowBlocks) {
  // 300-byte rows: the source is read in blocks of 13 rows and the output
  // written in blocks of 53 (four-tone) or 102 (one-bit) rows.
  constexpr int width = 300, height = 200;
  const auto tone = [](const int x, const int y) { return (x / 5 + y / 3) % 4; };
  for (const bool topDown : {true, false}) {
    const auto bmp =
        grayBmp(width, height, topDown, [&](int x, int y) { return static_cast<uint8_t>(tone(x, y) * 85); });
    // A centred 300x100 crop skips the first 50 rows, which span several blocks.
    for (const int outHeight : {height, 100}) {
      HalFile source(bmp);
      Sink out;
      ASSERT_TRUE(deriveThumbnailBmp(source, out, {width, outHeight, width, outHeight, ThumbnailFormat::FourTone}));
      ASSERT_EQ(out.bytes.size(), le32(out.bytes, 2));
      const int cropY = (height - outHeight) / 2;
      for (int y = 0; y < outHeight; ++y) {
        if ((y + cropY) % 3 != 1) continue;  // compare flat-block interiors only
        for (int x = 2; x < width; x += 5) {
          EXPECT_EQ(toneAt(out.bytes, x, y), tone(x, y + cropY)) << topDown << " " << x << "," << y;
        }
      }
    }
    // Black and white stripes pass through the one-bit ditherer unchanged.
    HalFile source(grayBmp(width, height, topDown,
                           [](int x, int y) { return static_cast<uint8_t>((x / 7 + y / 4) % 2 ? 255 : 0); }));
    Sink out;
    ASSERT_TRUE(deriveThumbnailBmp(source, out, {width, height, 0, 0, ThumbnailFormat::OneBit}));
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) ASSERT_EQ(toneAt(out.bytes, x, y), (x / 7 + y / 4) % 2) << x << "," << y;
    }
  }
}

TEST(BufferedPrint, GathersWritesAndReportsShortWritesAtFlush) {
  struct Counter : Print {
    std::vector<uint8_t> bytes;
    size_t calls = 0;
    size_t limit = SIZE_MAX;
    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t* data, size_t size) override {
      ++calls;
      const size_t accepted = std::min(size, limit - bytes.size());
      bytes.insert(bytes.end(), data, data + accepted);
      return accepted;
    }
  };
  std::vector<uint8_t> expected;
  const auto fill = [&](BufferedPrint& out) {
    for (int i = 0; i < 1000; ++i) {
      out.write(static_cast<uint8_t>(i));
      expected.push_back(static_cast<uint8_t>(i));
    }
    std::vector<uint8_t> rows(9000);
    for (size_t i = 0; i < rows.size(); ++i) rows[i] = static_cast<uint8_t>(i * 7);
    for (size_t at = 0; at < rows.size(); at += 300) out.write(rows.data() + at, 300);
    expected.insert(expected.end(), rows.begin(), rows.end());
  };
  {
    Counter sink;
    BufferedPrint out(sink, 4096);
    fill(out);
    EXPECT_TRUE(out.flushBuffer());
    EXPECT_EQ(sink.bytes, expected);
    EXPECT_EQ(sink.calls, 3u);  // 10,000 bytes in 4 KB blocks
  }
  {
    Counter sink;
    sink.limit = 5000;
    BufferedPrint out(sink, 4096);
    expected.clear();
    fill(out);
    EXPECT_FALSE(out.flushBuffer());
    EXPECT_EQ(out.write(uint8_t{1}), 0u);  // stays failed
  }
}
