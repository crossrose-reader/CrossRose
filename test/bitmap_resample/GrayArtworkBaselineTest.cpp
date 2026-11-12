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

// Captured screens (GfxRenderer::beginGrayArtworkCapture()): gray pixels of a
// picture are left white in the frame and marked in the overlay planes, dark
// gray in both, light gray in the MSB plane alone.
TEST(GrayArtworkCapture, GrayPixelsThatStayWhiteTurnBlackInTheBaseFrame) {
  // Pixels: dark gray, light gray, two white ones, then black.
  uint8_t frame[] = {0b11110000};
  uint8_t lsb[] = {0b10000000};
  uint8_t msb[] = {0b11000000};
  EXPECT_TRUE(resolveCapturedGrayArtwork(frame, lsb, msb, sizeof(frame)));
  // Both gray pixels are black in the base frame; the gray pass lightens them.
  EXPECT_EQ(frame[0], 0b00110000);
  EXPECT_EQ(lsb[0], 0b10000000);
  EXPECT_EQ(msb[0], 0b11000000);
}

TEST(GrayArtworkCapture, BlackDrawnOverAGrayPixelStaysBlack) {
  // An outline drawn over the cover's edge after the cover: the pixel is
  // black in the frame, so its marks no longer count.
  uint8_t frame[] = {0b01111111};
  uint8_t lsb[] = {0b11000000};
  uint8_t msb[] = {0b11000000};
  EXPECT_TRUE(resolveCapturedGrayArtwork(frame, lsb, msb, sizeof(frame)));
  EXPECT_EQ(frame[0], 0b00111111);
  EXPECT_EQ(lsb[0], 0b01000000);
  EXPECT_EQ(msb[0], 0b01000000);
}

TEST(GrayArtworkCapture, ReportsAScreenWithoutGray) {
  uint8_t frame[] = {0xFF, 0x0F};
  uint8_t lsb[] = {0x00, 0xF0};  // marked, but drawn over in black
  uint8_t msb[] = {0x00, 0xF0};
  EXPECT_FALSE(resolveCapturedGrayArtwork(frame, lsb, msb, sizeof(frame)));
  EXPECT_EQ(frame[0], 0xFF);
  EXPECT_EQ(frame[1], 0x0F);
  EXPECT_EQ(lsb[1], 0x00);
  EXPECT_EQ(msb[1], 0x00);
}

TEST(GrayArtworkCapture, PlanesHashTellsPicturesApart) {
  const uint8_t lsb[] = {0x00, 0x80, 0x00};
  const uint8_t msb[] = {0xC0, 0x80, 0x00};
  const uint8_t sameLsb[] = {0x00, 0x80, 0x00};
  const uint8_t sameMsb[] = {0xC0, 0x80, 0x00};
  EXPECT_EQ(grayArtworkPlanesHash(lsb, msb, 3), grayArtworkPlanesHash(sameLsb, sameMsb, 3));
  // Swapping a pixel between the planes changes its tone, and the hash.
  EXPECT_NE(grayArtworkPlanesHash(lsb, msb, 3), grayArtworkPlanesHash(msb, lsb, 3));
  const uint8_t moved[] = {0x60, 0x80, 0x00};
  EXPECT_NE(grayArtworkPlanesHash(lsb, msb, 3), grayArtworkPlanesHash(lsb, moved, 3));
}
