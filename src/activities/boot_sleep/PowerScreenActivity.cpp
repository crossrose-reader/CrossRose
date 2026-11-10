#include "PowerScreenActivity.h"

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <I18n.h>

#include "SleepActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr int HINT_SIDE_MARGIN = 20;
constexpr int HINT_MAX_LINES = 3;
}  // namespace

void PowerScreenActivity::onEnter() {
  Activity::onEnter();

  // Like the sleep screen, this draws outside ActivityManager's render path:
  // use the normal palette and portrait layout whatever the last screen used.
  display.setInverted(false);
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  if (mode == Mode::Restart) {
    SleepActivity::drawLogoScreen(renderer, tr(STR_RESTARTING));
    // The boot splash replaces this within seconds, so skip the slower clean refresh.
    renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    return;
  }

  SleepActivity::drawLogoScreen(renderer, tr(STR_POWERED_OFF));
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int hintY = pageHeight / 2 + 125;
  const Rect hintBounds{HINT_SIDE_MARGIN, hintY, pageWidth - HINT_SIDE_MARGIN * 2, pageHeight - hintY};
  UITheme::drawCenteredWrappedText(renderer, hintBounds, SMALL_FONT_ID, hintY, tr(STR_POWER_ON_HINT), HINT_MAX_LINES);
  // This frame stays on the panel while the device is off: use the clean
  // single-pass refresh the sleep screens use, then power the panel down.
  renderer.displayBuffer(HalDisplay::HALF_REFRESH, true);
}
