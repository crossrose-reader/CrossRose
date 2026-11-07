#pragma once

#include <cstddef>

// Version rules for "Check for updates". Plain C string code with no Arduino
// or ESP-IDF calls, so the native test suite (test/ota_version) checks them
// on the host.
namespace ota_version {

constexpr size_t SEGMENT_COUNT = 4;

struct ParsedVersion {
  int segments[SEGMENT_COUNT] = {0, 0, 0, 0};
  bool valid = false;
  // Built before the stable release with the same number: a nightly, a build
  // from source, or a release candidate. See isPrerelease() in OtaVersion.cpp.
  bool prerelease = false;
};

// Reads up to four numbers ("0.1.0", "v0.1.0", "1.4.0.1") and notes whether
// the rest marks a pre-release. Not valid when the text does not start with a
// number after an optional "v", like the "dev" fallback when the build set
// no version.
ParsedVersion parse(const char* version);

// 1 when `latest` is newer than `current`, -1 when it is older, 0 when they
// are the same or either can't be parsed. A stable release is newer than its
// own pre-releases: "v0.1.0" is newer than "0.1.0-nightly.20261201".
int compare(const char* latest, const char* current);

}  // namespace ota_version
