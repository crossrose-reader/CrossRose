#include <GrayArtworkBaseline.h>
#include <gtest/gtest.h>

#include <cstdint>

// Framebuffer bits: 1 is white, 0 is black. A differential refresh drives a
// pixel only when the baseline and the next frame disagree.
TEST(GrayArtworkBaseline, GrayPixelsAlwaysDisagreeWithTheNextFrame) {
  //                    gray pixels ----v v v v
  uint8_t shown[] = {0b11110000, 0b00001111, 0x00, 0xFF};
  const uint8_t gray[] = {0b00001111, 0b00001111, 0xFF, 0xFF};
  const uint8_t next[] = {0b10100101, 0b10100101, 0x00, 0xFF};
  buildGrayArtworkBaseline(shown, gray, next, sizeof(shown));

  for (size_t i = 0; i < sizeof(shown); ++i) {
    // Every gray pixel is driven, whichever colour it becomes.
    EXPECT_EQ((shown[i] ^ next[i]) & gray[i], gray[i]) << i;
  }
  // Pixels that were not gray keep what the controller already believes.
  EXPECT_EQ(shown[0] & 0xF0, 0xF0);
  EXPECT_EQ(shown[1] & 0xF0, 0x00);
}

TEST(GrayArtworkBaseline, LeavesAFrameWithoutGrayUntouched) {
  uint8_t shown[] = {0x5A, 0xC3};
  const uint8_t gray[] = {0x00, 0x00};
  const uint8_t next[] = {0xFF, 0x00};
  buildGrayArtworkBaseline(shown, gray, next, sizeof(shown));
  EXPECT_EQ(shown[0], 0x5A);
  EXPECT_EQ(shown[1], 0xC3);
}
