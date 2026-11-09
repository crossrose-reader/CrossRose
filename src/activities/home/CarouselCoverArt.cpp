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

struct Target {
  int width;
  int height;
  // Zero keeps the output's aspect; the side thumbnail is squeezed instead.
  int cropAspectWidth;
  int cropAspectHeight;
  ThumbnailFormat format;
};

constexpr Target kCenter{LyraCarouselTheme::kCenterThumbW, LyraCarouselTheme::kCenterThumbH, 0, 0,
                         ThumbnailFormat::OneBit};
constexpr Target kCenterFourTone{LyraCarouselTheme::kCenterThumbW, LyraCarouselTheme::kCenterThumbH, 0, 0,
                                 ThumbnailFormat::FourTone};
constexpr Target kSide{LyraCarouselTheme::kSideThumbW, LyraCarouselTheme::kSideThumbH, LyraCarouselTheme::kSideCoverW,
                       LyraCarouselTheme::kSideCoverH, ThumbnailFormat::OneBit};
constexpr Target kSideFourTone{LyraCarouselTheme::kSideThumbW, LyraCarouselTheme::kSideThumbH,
                               LyraCarouselTheme::kSideCoverW, LyraCarouselTheme::kSideCoverH,
                               ThumbnailFormat::FourTone};

std::string pathFor(const RecentBook& book, const Target& target) {
  // No legacy fallback: these are the names thumbnails are written under.
  return thumbnailVariantPath(UITheme::getCoverThumbPath(book.coverBmpPath, target.width, target.height, false),
                              target.format);
}

bool exists(const RecentBook& book, const Target& target) {
  const std::string path = pathFor(book, target);
  return !path.empty() && Storage.exists(path.c_str());
}

bool derive(const RecentBook& book, const std::string& sourcePath, const Target& target) {
  const std::string path = pathFor(book, target);
  if (path.empty()) return false;
  if (Storage.exists(path.c_str())) return true;

  // SdFat cannot rename over a file, and a thumbnail that exists is trusted,
  // so write to a temporary name and publish only a complete file.
  const std::string tmpPath = path + ".tmp";
  if (Storage.exists(tmpPath.c_str())) Storage.remove(tmpPath.c_str());

  FsFile source;
  if (!Storage.openFileForRead("HOME", sourcePath, source)) {
    LOG_ERR("HOME", "carousel: cannot open thumbnail source %s", sourcePath.c_str());
    return false;
  }
  FsFile out;
  if (!Storage.openFileForWrite("HOME", tmpPath, out)) {
    LOG_ERR("HOME", "carousel: cannot create %s", tmpPath.c_str());
    source.close();
    return false;
  }
  const ThumbnailSpec spec{target.width, target.height, target.cropAspectWidth, target.cropAspectHeight, target.format};
  bool ok = deriveThumbnailBmp(source, out, spec);
  source.close();
  ok = out.close() && ok;
  if (ok && !Storage.rename(tmpPath.c_str(), path.c_str())) {
    LOG_ERR("HOME", "carousel: cannot publish %s", path.c_str());
    ok = false;
  }
  if (!ok) Storage.remove(tmpPath.c_str());
  return ok;
}

}  // namespace

std::string centerThumbPath(const RecentBook& book) { return pathFor(book, kCenter); }

std::string sideThumbPath(const RecentBook& book) { return pathFor(book, kSide); }

bool needsThumbnails(const RecentBook& book) {
  if (book.coverBmpPath.empty()) return false;
  if (!exists(book, kCenter) || !exists(book, kSide)) return true;
  return fourToneEnabled() && !exists(book, kSideFourTone);
}

bool deriveMissing(const RecentBook& book, const std::string& sourcePath, const bool graySource) {
  bool ok = true;
  if (graySource) {
    ok = derive(book, sourcePath, kCenter) && ok;
    if (fourToneEnabled()) ok = derive(book, sourcePath, kCenterFourTone) && ok;
  }
  ok = derive(book, sourcePath, kSide) && ok;
  if (fourToneEnabled()) ok = derive(book, sourcePath, kSideFourTone) && ok;
  return ok;
}

}  // namespace CarouselCoverArt
