#pragma once
#include <HalDisplay.h>
#include <Memory.h>
#include <ScreenInsets.h>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "./FileBrowserActivity.h"
#include "QuickActions.h"
#include "activities/Activity.h"
#include "activities/reader/BookReadingStats.h"
#include "activities/reader/GlobalReadingStats.h"
#include "components/CoverGridHomeUi.h"
#include "components/OptionPopup.h"
#include "util/ButtonNavigator.h"

struct RecentBook;
struct Rect;

class HomeActivity final : public Activity {
 public:
  // Keep one rendered carousel frame in RAM. Additional frames remain available
  // through the SD snapshot cache and are paged in on demand.
  static constexpr int kCarouselFrameCount = 1;
  // Must be >= LyraCarouselMetrics::values.homeRecentBooksCount (asserted in .cpp)
  static constexpr int kMaxCachedBooks = 3;

 private:
#ifdef SIMULATOR
  friend struct BookmarkListSmokeTest;
  friend struct PluginCatalogSmokeTest;
#endif
  ButtonNavigator buttonNavigator;
  std::unique_ptr<CoverGridHomeUi> coverGridUi;
  bool gridHasContinueReading = false;
  int selectorIndex = 0;
  int lastCarouselBookIndex = 0;  // remembered position when leaving carousel row
  int carouselCoverTouchDownIndex = -1;
  bool carouselCoverTouchDownWasSelected = false;
  // Touch menus use a momentary pressed state. Keep it separate from the
  // keyboard selection so a returned Home screen cannot retain an icon tint.
  int carouselMenuTouchDownIndex = -1;
  bool recentsLoading = false;
  bool recentsLoaded = false;
  // Silent restarts keep the panel's previous frame. The first Home paint may
  // need a clean waveform so X4 panels do not diff against a WiFi screen.
  HalDisplay::RefreshMode initialRefreshMode = HalDisplay::FAST_REFRESH;
  bool hasReadingStats = false;
  bool hasBookmarks = false;
  bool hasClippings = false;
  bool hasPlugins = false;
  bool minimalMenuOpen = false;
  bool minimalSuppressInitialFrontRelease = false;
  bool homeBookSwapLongPressHandled = false;
  bool quickActionsLongPowerHandled = false;
  int minimalMenuIndex = 0;
  int minimalHomeNavIndex = -1;
  bool coverRendered = false;      // Track if cover has been rendered once
  bool coverBufferStored = false;  // Track if cover buffer is stored
  // Cached cover pixels already include Dark Mode's image-polarity correction.
  // They must be redrawn rather than reused when the output polarity changes.
  bool coverBufferInverted = false;
  // Home can be entered while Back is still held (e.g. leaving Settings with
  // Back): ignore that stale release until a fresh press is seen here.
  bool backPressSeen = false;
  OptionPopup quickActionsPopup;
  uint8_t* coverBuffer = nullptr;  // HomeActivity's own buffer for cover image
  size_t coverBufferSize = 0;      // Bytes allocated to coverBuffer
  // A cover snapshot taken during a four-tone capture (FourToneCovers.h) has
  // its gray pixels white; these PSRAM copies of the same region of the two
  // capture planes hold their marks, LSB region then MSB region.
  HeapByteBuffer coverPlanes;
  size_t coverPlanesSize = 0;
  bool coverBufferFourTone = false;
  // Logical rect last passed to drawRecentBookCover. The cover snapshot only
  // needs to cover this region, not the entire framebuffer, so we cache the
  // tile instead of all 48 KB. Set in render() before the call.
  int coverRectX = 0;
  int coverRectY = 0;
  int coverRectW = 0;
  int coverRectH = 0;
  float currentBookProgressPercent = -1.0f;
  std::string currentBookChapterTitle;
  BookReadingStats currentBookStats;
  GlobalReadingStats globalStats;
  GlobalReadingStats allDevicesGlobalStats;
  bool showAllDevicesStats = false;

  // Per-book stats and progress cached at onEnter() to avoid SD reads during navigation.
  std::array<BookReadingStats, kMaxCachedBooks> cachedBookStats{};
  std::array<float, kMaxCachedBooks> cachedBookProgress{};
  bool bookStatsCached = false;

  uint8_t* carouselFrames[kCarouselFrameCount] = {};
  bool carouselFramesReady = false;

  // Four-tone cover art (Carousel on CROSSINK_APP_CAP_GRAY_COVER_ART builds).
  // A position's artwork is drawn once into PSRAM frames: the B/W base (gray
  // pixels black) and the two overlay planes. The planes leave the centre
  // outline untinted, and the outline is thicker while the carousel row is
  // selected, so each selection state gets its own pair; the base serves both.
  // Showing a position again copies its frames instead of reading the
  // thumbnails from SD. Up to 5 x 48 KB per position, kept until Home exits.
  struct FourToneFrames {
    // Whether the centre cover has a four-tone thumbnail: -1 not checked yet.
    int8_t hasArt = -1;
    bool baseReady = false;
    std::array<bool, 2> planesReady{};  // indexed by selection state
    HeapByteBuffer base;
    std::array<HeapByteBuffer, 2> lsb;
    std::array<HeapByteBuffer, 2> msb;
  };
  // The frames one render shows; all null when four tones are not available.
  struct FourTonePicture {
    const uint8_t* base = nullptr;
    const uint8_t* lsb = nullptr;
    const uint8_t* msb = nullptr;
  };
  std::array<FourToneFrames, kMaxCachedBooks> fourToneFrames;
  uint64_t fourToneFramesKey = 0;
  // What the four-tone picture on the panel shows; a render of the same state
  // keeps it instead of repeating both refreshes.
  bool fourToneShown = false;
  int fourToneCenterIdx = -1;
  bool fourToneSelected = false;
  uint64_t fourToneKeyHash = 0;
  // Set for the rest of the visit once four tones could not be shown (no
  // PSRAM for the frames or the capture planes, or the pass could not start),
  // on every theme.
  bool fourToneUnavailable = false;
  bool carouselFramesInverted = false;
  bool carouselWarmupPending = false;
  ScreenInsets insetsBeforeFrontlightPanel;
  uint8_t themeBeforeFrontlightPanel = 0;
  uint8_t scaleBeforeFrontlightPanel = 0;
  uint32_t filenameFontBeforeFrontlightPanel = 0;
  uint8_t statusSizeBeforeFrontlightPanel = 0;

  std::vector<RecentBook> recentBooks;
  const HomeMenuItem initialMenuItem;
  std::string initialBookPath;

  void onSelectBook(const std::string& path);
  void onFileBrowserOpen();
  void onMinimalBrowseOpen();
  void onContinueReading();
  void onLibraryOpen();
  void onSettingsOpen();
  void onFileTransferOpen();
  void onPluginsOpen();
  void onReadingStatsOpen();
  void onSavedItemsOpen();
  void onPowerMenuOpen();

  int getMenuItemCount() const;
  bool storeCoverBuffer();    // Store frame buffer for cover image
  bool restoreCoverBuffer();  // Restore frame buffer from stored cover
  void freeCoverBuffer();     // Free the stored cover buffer
  void invalidateCoverCache();
  void invalidatePolarityMismatchedCaches();
  // renderViewedFrame = false only prepares the snapshot cache and leaves the
  // viewed position's frame to be loaded when it is first needed.
  void preRenderCarouselFrames(bool renderViewedFrame = true);
  void freeCarouselFrames();
  bool allocateCarouselFrameSlots(int targetFrameCount);
  bool saveCarouselFrameToDisk(uint64_t cacheKeyHash, int bookCount, int bookIdx, int slotIdx);
  bool loadCarouselFrameFromDisk(uint64_t cacheKeyHash, int bookCount, int bookIdx, int slotIdx);
  int chooseCarouselEvictionSlot(int centerIdx, int bookCount,
                                 std::optional<int> protectedBookIdx = std::nullopt) const;
  void renderCarouselFrameToCurrentBuffer(int bookIdx);
  // Starts a four-tone capture for this render on themes drawn in one pass
  // (every theme but Carousel); false when covers stay black and white.
  bool beginFourToneCapture();
  FourToneFrames* fourToneFramesFor(int centerIdx);
  bool canShowFourToneCovers(int centerIdx);
  FourTonePicture prepareFourToneFrames(const Rect& coverRect, int centerIdx, bool selected);
  void invalidateFourToneFrames();
  void resetFourToneCovers();
  void renderCarouselFrame(int bookIdx, int slotIdx);
  int getHighlightedBookIndex() const;
  int getVisibleRecentBookCount() const;
  bool canSwapHomeBook() const;
  void showNextRecentBookOnHome();
  void updateHighlightedBookContext(bool allowChapterTitleRead = true);
  void loadRecentBooks(int maxBooks);
  // Returns true when Home needs a repaint for new artwork or to clear progress.
  bool loadCoverGridThumbnails();
  void activateCoverGridSelection();
  void loadAllBookStats();
  void loadRecentCovers(int coverHeight);

 public:
  explicit HomeActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                        HomeMenuItem initialMenuItemValue = HomeMenuItem::NONE,
                        HalDisplay::RefreshMode initialRefreshModeValue = HalDisplay::FAST_REFRESH,
                        std::string initialBookPathValue = {})
      : Activity("Home", renderer, mappedInput),
        initialRefreshMode(initialRefreshModeValue),
        initialMenuItem(initialMenuItemValue),
        initialBookPath(std::move(initialBookPathValue)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isHomeActivity() const override { return true; }
  bool allowPowerAsConfirmInReaderMode() const override { return quickActionsPopup.isActive(); }
  bool blocksGlobalInput() const override { return quickActionsPopup.isActive(); }
  bool handleShortcutAction(CrossPointSettings::SHORT_PWRBTN action) override;
  std::string getCurrentBookPath() const override;
  std::string getCurrentBookTitle() const override;
  std::unique_ptr<Activity> createFrontlightReadingStatsActivity() override;
  void onFrontlightPanelOpened() override;
  void onFrontlightPanelClosed() override;
};
