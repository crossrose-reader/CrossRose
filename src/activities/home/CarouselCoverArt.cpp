#include "CarouselCoverArt.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ThumbnailDeriver.h>
#include <ThumbnailFormat.h>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/themes/lyra/LyraCarouselTheme.h"

namespace CarouselCoverArt {
namespace {

// The centre thumbnail keeps its own aspect. The side thumbnail takes the
// side cover's framing and squeezes it to the drawn width.
constexpr ThumbnailSpec kCenter{LyraCarouselTheme::kCenterThumbW, LyraCarouselTheme::kCenterThumbH, 0, 0,
                                ThumbnailFormat::OneBit};
constexpr ThumbnailSpec kCenterFourTone{LyraCarouselTheme::kCenterThumbW, LyraCarouselTheme::kCenterThumbH, 0, 0,
                                        ThumbnailFormat::FourTone};
constexpr ThumbnailSpec kSide{LyraCarouselTheme::kSideThumbW, LyraCarouselTheme::kSideThumbH,
                              LyraCarouselTheme::kSideCoverW, LyraCarouselTheme::kSideCoverH, ThumbnailFormat::OneBit};
constexpr ThumbnailSpec kSideFourTone{LyraCarouselTheme::kSideThumbW, LyraCarouselTheme::kSideThumbH,
                                      LyraCarouselTheme::kSideCoverW, LyraCarouselTheme::kSideCoverH,
                                      ThumbnailFormat::FourTone};

std::string pathFor(const RecentBook& book, const ThumbnailSpec& spec) {
  // No legacy fallback: these are the names thumbnails are written under.
  return thumbnailVariantPath(UITheme::getCoverThumbPath(book.coverBmpPath, spec.width, spec.height, false),
                              spec.format);
}

bool exists(const RecentBook& book, const ThumbnailSpec& spec) {
  const std::string path = pathFor(book, spec);
  return !path.empty() && Storage.exists(path.c_str());
}

enum class Derived : uint8_t { Existing, Written, Failed };

Derived derive(const RecentBook& book, const std::string& sourcePath, const ThumbnailSpec& spec) {
  const std::string path = pathFor(book, spec);
  if (path.empty()) {
    LOG_ERR("HOME", "carousel: no thumbnail path for %s", book.path.c_str());
    return Derived::Failed;
  }
  if (Storage.exists(path.c_str())) return Derived::Existing;

  // SdFat cannot rename over a file, and a thumbnail that exists is trusted,
  // so write to a temporary name and publish only a complete file.
  const std::string tmpPath = path + ".tmp";
  if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());

  FsFile source;
  if (!Storage.openFileForRead("HOME", sourcePath, source)) {
    LOG_ERR("HOME", "carousel: cannot open thumbnail source %s", sourcePath.c_str());
    return Derived::Failed;
  }
  FsFile out;
  if (!Storage.openFileForWrite("HOME", tmpPath, out)) {
    LOG_ERR("HOME", "carousel: cannot create %s", tmpPath.c_str());
    source.close();
    return Derived::Failed;
  }
  bool ok = deriveThumbnailBmp(source, out, spec);
  source.close();
  ok = out.close() && ok;
  if (ok && !Storage.rename(tmpPath.c_str(), path.c_str())) {
    LOG_ERR("HOME", "carousel: cannot publish %s", path.c_str());
    ok = false;
  }
  if (!ok) Storage.remove(tmpPath.c_str());
  return ok ? Derived::Written : Derived::Failed;
}

}  // namespace

std::string centerThumbPath(const RecentBook& book) { return pathFor(book, kCenter); }

std::string sideThumbPath(const RecentBook& book) { return pathFor(book, kSide); }

bool needsThumbnails(const RecentBook& book) {
  if (book.coverBmpPath.empty()) return false;
  if (!exists(book, kCenter) || !exists(book, kSide)) return true;
  return fourToneEnabled() && !exists(book, kSideFourTone);
}

DeriveResult deriveMissing(const RecentBook& book, const std::string& sourcePath, const Source source) {
  DeriveResult result;
  const auto want = [&](const ThumbnailSpec& spec) {
    const Derived derived = derive(book, sourcePath, spec);
    if (derived == Derived::Written) result.changed = true;
    if (derived == Derived::Failed) result.complete = false;
    return derived != Derived::Failed;
  };

  // Black and white first: every build draws them.
  if (source == Source::Gray) want(kCenter);
  want(kSide);
  if (!fourToneEnabled() || source == Source::OneBitStandIn) return result;

  // The four-tone side goes last: needsThumbnails() reads it as the mark that
  // the four-tone set is complete, so it waits for the four-tone centre.
  if (source == Source::Gray && !want(kCenterFourTone)) return result;
  want(kSideFourTone);
  return result;
}

}  // namespace CarouselCoverArt
