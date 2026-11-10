#pragma once
#include <string>
#include <utility>

#include "activities/Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool canSnapshotOverlayBackground,
                         std::string currentBookPath = {}, bool fromTimeout = false,
                         GfxRenderer::Orientation sleepPopupOrientation = GfxRenderer::Orientation::Portrait)
      : Activity("Sleep", renderer, mappedInput),
        canSnapshotOverlayBackground(canSnapshotOverlayBackground),
        currentBookPath(std::move(currentBookPath)),
        fromTimeout(fromTimeout),
        sleepPopupOrientation(sleepPopupOrientation) {}
  void onEnter() override;
  // Whether this screen is independent of the outgoing activity's saves.
  bool rendersBeforeExit() const;
  // Clears the frame and draws the logo, the reader's name and `status`, as on
  // the default sleep screen. PowerScreenActivity reuses it so both screens
  // share one copy of the logo bitmap in flash.
  static void drawLogoScreen(const GfxRenderer& renderer, const char* status);

 private:
#ifdef SIMULATOR
  friend struct UpstreamFeatureSmokeTest;
#endif
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderReadingStatsSleepScreen() const;
  void renderMinimalSleepScreen() const;
  void renderMinimalStatsSleepScreen() const;
  void renderDashboardSleepScreen() const;
  struct BitmapPlacement {
    int x, y, width, height;
    float cropX, cropY;
  };
  BitmapPlacement prepareSleepBitmap(Bitmap& bitmap) const;
  bool drawSleepBitmap(Bitmap& bitmap, const BitmapPlacement& placement) const;
  bool renderBitmapSleepScreen(Bitmap& bitmap) const;
  void renderLastScreenSleepScreen() const;
  void renderBlankSleepScreen() const;
  void renderOverlaySleepScreen(Bitmap* cover = nullptr) const;
  bool canSnapshotOverlayBackground = false;
  bool overlayBackgroundBufferStored = false;
  std::string currentBookPath;
  bool fromTimeout = false;
  GfxRenderer::Orientation sleepPopupOrientation = GfxRenderer::Orientation::Portrait;
};
