

#pragma once

#include "components/themes/lyra/LyraTheme.h"

class GfxRenderer;

// Lyra Carousel theme metrics (zero runtime cost)
namespace LyraCarouselMetrics {
constexpr ThemeMetrics makeValues() {
  ThemeMetrics v = LyraMetrics::values;
  v.listRowHeight = 35;
  v.menuRowHeight = 64;
  v.menuSpacing = 8;
  v.homeTopPadding = 28;
  v.homeCoverHeight = 600;
  v.homeCoverTileHeight = 660;
  v.homeRecentBooksCount = 3;
  v.keyboardKeyHeight = 56;
  v.keyboardCenteredText = true;
  return v;
}

constexpr ThemeMetrics values = makeValues();
}  // namespace LyraCarouselMetrics

class LyraCarouselTheme : public LyraTheme {
 public:
  // Max cache geometry for the carousel artwork area.
  static constexpr int kCenterCoverW = 340;
  static constexpr int kCenterCoverH = LyraCarouselMetrics::values.homeCoverHeight - 60;  // 540
  static constexpr int kCenterCoverVisualInset = 10;
  static constexpr int kBaseDisplayCenterW = (kCenterCoverW * 86) / 100;
  static constexpr int kBaseDisplayCenterH = (kCenterCoverH * 86) / 100;
  static constexpr int kDisplayCenterW =
      ((kBaseDisplayCenterW + 24) < kCenterCoverW) ? (kBaseDisplayCenterW + 24) : kCenterCoverW;
  static constexpr int kDisplayCenterH =
      ((kBaseDisplayCenterH + 24) < kCenterCoverH) ? (kBaseDisplayCenterH + 24) : kCenterCoverH;
  // Actual center image rect after the visual inset; using this for the cached
  // thumbnail avoids scaling an already-dithered BMP at draw time.
  static constexpr int kCenterThumbW = kDisplayCenterW - kCenterCoverVisualInset * 2;  // 296
  static constexpr int kCenterThumbH = kDisplayCenterH - kCenterCoverVisualInset * 2;  // 468
  // Framing of a side cover: the centred kSideCoverW:kSideCoverH crop of the cover.
  static constexpr int kSideCoverW = 200;
  static constexpr int kSideCoverH = LyraCarouselMetrics::values.homeCoverHeight - 210;  // 390
  // Drawn geometry of the nearer side covers, which lean away from the centre.
  static constexpr int kNearSideW = (kBaseDisplayCenterW * 26) / 100;       // 75
  static constexpr int kNearSideInnerH = (kBaseDisplayCenterH * 90) / 100;  // 417
  static constexpr int kNearSideOuterH = (kBaseDisplayCenterH * 82) / 100;  // 380
  // Side thumbnails hold that crop already squeezed to the drawn width, so the
  // dither pattern is made at the size it is shown. Drawing maps columns 1:1
  // and only drops rows towards the shorter edge.
  static constexpr int kSideThumbW = kNearSideW;
  static constexpr int kSideThumbH = kNearSideInnerH;
  // Tallest 8-bit source both thumbnails are derived from: kCenterThumbW wide
  // with the side's narrower kSideCoverW:kSideCoverH framing, so a narrow
  // cover keeps the rows its side crop needs.
  static constexpr int kThumbSourceMaxH = (kCenterThumbW * kSideCoverH + kSideCoverW - 1) / kSideCoverW;  // 578

  // OneBit is the ordinary black-and-white artwork. FourToneBase and
  // FourTonePlane are the passes of a four-tone picture (see
  // GfxRenderer::beginGrayArtwork): the base draws every gray pixel black,
  // and the plane pass, run once per grayscale render mode, marks the pixels
  // to lighten. Covers without a four-tone thumbnail stay black and white.
  enum class CoverArtPass : uint8_t { OneBit, FourToneBase, FourTonePlane };
  // Draws the covers and, in the two black-and-white passes, the title and
  // position dots. `selected` is whether the centre cover carries the thick
  // selection outline, which the plane pass must leave untinted.
  void drawCoverArt(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks, int centerIdx,
                    CoverArtPass pass, bool selected) const;
  // True when the centre cover has a four-tone thumbnail to show.
  static bool hasFourToneCoverArt(const std::vector<RecentBook>& recentBooks, int centerIdx);

  static void setPreRenderIndex(int idx);
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           const std::function<bool()>& storeCoverBuffer, const BookReadingStats* stats = nullptr,
                           float progressPercent = -1.0f, const GlobalReadingStats* globalStats = nullptr,
                           const char* currentChapterTitle = nullptr) const override;
  // Live footer, shared by direct drawing and cached-artwork restoration.
  void drawReadingProgress(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const BookReadingStats* stats, float progressPercent) const;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<const char*(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  // Full touch strip used by the carousel menu, including the label below the icons.
  // Home uses this same geometry to consume swipes that start over the menu.
  static Rect buttonMenuTouchRect(const GfxRenderer& renderer, int buttonCount);
  void registerButtonMenuTouchTargets(const GfxRenderer& renderer, int buttonCount) const;
  void drawCarouselBorder(GfxRenderer& renderer, Rect coverRect, const std::vector<RecentBook>& recentBooks,
                          int centerIdx, bool inCarouselRow) const override;
  void drawList(const GfxRenderer& renderer, Rect rect, int itemCount, int selectedIndex,
                const std::function<std::string(int index)>& rowTitle,
                const std::function<std::string(int index)>& rowSubtitle = {},
                const std::function<UIIcon(int index)>& rowIcon = {},
                const std::function<std::string(int index)>& rowValue = {}, bool highlightValue = false,
                const std::function<bool(int index)>& rowDimmed = {},
                const std::function<bool(int index)>& isHeader = {}, int rowHeightScale = 1,
                bool showSelection = true) const override;
};
