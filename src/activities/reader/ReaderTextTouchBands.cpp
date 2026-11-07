#include "ReaderTextTouchBands.h"

#include <Epub/Page.h>
#include <GfxRenderer.h>

#include <algorithm>
#include <limits>

#include "CrossPointSettings.h"
#include "PageWordGeometry.h"

namespace {
// WordSelectNavigator::selectWordAtPoint pads each word by 6 px sideways and max(4, h / 4)
// vertically. Add a little more so font-resolution differences can never reject a real word.
constexpr int HORIZONTAL_PAD = 6 + 4;
constexpr int EXTRA_VERTICAL_PAD = 4;

int16_t toInt16(const int value) {
  return static_cast<int16_t>(std::clamp(value, static_cast<int>(std::numeric_limits<int16_t>::min()),
                                         static_cast<int>(std::numeric_limits<int16_t>::max())));
}
}  // namespace

void ReaderTextTouchBands::build(const Page& page, const GfxRenderer& renderer, const int fontId, const int marginTop,
                                 const int marginLeft) {
  beginPage();
  // Word selection falls back to the reader font's line height; take the larger of it and the
  // page's render font so the band is never shorter than a word's hit box.
  const int fallbackLineHeight =
      std::max(renderer.getLineHeight(fontId), renderer.getLineHeight(SETTINGS.getReaderFontId()));

  struct Context {
    ReaderTextTouchBands* bands;
    const GfxRenderer* renderer;
    int fontId;
    int marginTop;
    int marginLeft;
    int fallbackLineHeight;
  } context{this, &renderer, fontId, marginTop, marginLeft, fallbackLineHeight};

  page.forEachTextLine(
      [](const PageTextLine& line, void* raw) {
        const auto& ctx = *static_cast<Context*>(raw);
        const TextBlock* block = line.block;
        if (!block || block->wordCount() == 0) return true;

        // Only the outermost words set the horizontal extent, so measure just those two. The same pass
        // finds the smallest word ascender: word selection drops each smaller word by
        // maxAscender - its ascender (mixed font sizes in one line), so the band must reach that low.
        const int lineFontId = block->resolvedFontId(*ctx.renderer, ctx.fontId);
        uint16_t leftIndex = 0;
        uint16_t rightIndex = 0;
        int minAscender = ctx.renderer->getFontAscenderSize(block->wordFontId(*ctx.renderer, lineFontId, 0));
        for (uint16_t i = 1; i < block->wordCount(); ++i) {
          if (block->wordXpos(i) < block->wordXpos(leftIndex)) leftIndex = i;
          if (block->wordXpos(i) > block->wordXpos(rightIndex)) rightIndex = i;
          minAscender =
              std::min(minAscender, ctx.renderer->getFontAscenderSize(block->wordFontId(*ctx.renderer, lineFontId, i)));
        }
        const PageWordGeometry leftWord = pageWordGeometry(*ctx.renderer, ctx.fontId, line, *block, leftIndex);
        const PageWordGeometry rightWord = pageWordGeometry(*ctx.renderer, ctx.fontId, line, *block, rightIndex);
        const int lineX = ctx.marginLeft + line.xPos;
        const int left = lineX + leftWord.xOffset;
        const int right = lineX + rightWord.xOffset + std::max(rightWord.width, 0);

        const int maxAscender = block->maxAscender(*ctx.renderer, lineFontId);
        const int rubyShift = block->getRubyShift(maxAscender);
        const int maxWordYOffset = std::max(0, maxAscender - minAscender);
        const int height = std::max(line.lineHeight, ctx.fallbackLineHeight);
        const int verticalPad = std::max(4, height / 4) + EXTRA_VERTICAL_PAD;
        const int top = ctx.marginTop + line.yPos;

        ctx.bands->addLine(left - HORIZONTAL_PAD, top - verticalPad, right + HORIZONTAL_PAD,
                           top + rubyShift + maxWordYOffset + height + verticalPad);
        return true;
      },
      &context);
}

void ReaderTextTouchBands::addLine(const int left, const int top, const int right, const int bottom) {
  if (!known_ || right <= left || bottom <= top) return;

  if (count_ > 0) {
    Band& last = bands_[count_ - 1];
    if (top <= last.bottom && bottom >= last.top) {
      last.left = std::min(last.left, toInt16(left));
      last.top = std::min(last.top, toInt16(top));
      last.right = std::max(last.right, toInt16(right));
      last.bottom = std::max(last.bottom, toInt16(bottom));
      return;
    }
  }

  if (count_ >= MAX_BANDS) {
    // Too fragmented to describe; fall back to treating every touch as a possible word.
    known_ = false;
    return;
  }
  bands_[count_++] = Band{toInt16(left), toInt16(top), toInt16(right), toInt16(bottom)};
}

bool ReaderTextTouchBands::mayContainWord(const int x, const int y) const {
  if (!known_) return true;
  for (uint8_t i = 0; i < count_; ++i) {
    const Band& band = bands_[i];
    if (x >= band.left && x <= band.right && y >= band.top && y <= band.bottom) return true;
  }
  return false;
}
