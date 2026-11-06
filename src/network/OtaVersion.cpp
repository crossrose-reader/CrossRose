#include "OtaVersion.h"

#include <strings.h>

#include <cstring>

namespace ota_version {
namespace {

bool isDigit(const char c) { return c >= '0' && c <= '9'; }

bool startsWithNumberAfterOptionalV(const char* version) {
  if (version == nullptr) return false;
  if ((version[0] == 'v' || version[0] == 'V') && isDigit(version[1])) return true;
  return isDigit(version[0]);
}

// A pre-release is a build made before the stable release with the same
// number:
// - a nightly: "0.1.0-nightly.20261201", "0.1.0-nightly.20261201-x4-pro";
// - a build from source, which carries '+' and the branch or commit:
//   "0.1.0-dev+main", "0.1.0-dev+main-x4-pro", debug "0.1.0-main+a1b2c";
// - a release candidate: "0.2.0-rc1", CrossInk's "1.6.1-RC".
// Stable builds read "0.1.0" or "0.1.0-x4-pro" and never contain '+':
// git_branch.py's sanitizer turns '+' in release versions into '-'.
bool isPrerelease(const char* version) {
  if (strchr(version, '+') != nullptr) return true;
  for (const char* p = version; *p != '\0'; ++p) {
    if (*p != '-') continue;
    if (strncasecmp(p + 1, "nightly", 7) == 0 || strncasecmp(p + 1, "rc", 2) == 0) return true;
  }
  return false;
}

}  // namespace

ParsedVersion parse(const char* version) {
  ParsedVersion parsed;
  if (!startsWithNumberAfterOptionalV(version)) return parsed;

  const char* p = version;
  if (p[0] == 'v' || p[0] == 'V') ++p;

  size_t segmentIndex = 0;
  while (segmentIndex < SEGMENT_COUNT) {
    if (!isDigit(*p)) return parsed;

    int value = 0;
    while (isDigit(*p)) {
      value = value * 10 + (*p - '0');
      ++p;
    }
    parsed.segments[segmentIndex] = value;
    ++segmentIndex;

    if (*p != '.') break;
    ++p;
  }

  parsed.valid = true;
  parsed.prerelease = isPrerelease(version);
  return parsed;
}

int compare(const char* latest, const char* current) {
  const ParsedVersion latestParsed = parse(latest);
  const ParsedVersion currentParsed = parse(current);
  if (!latestParsed.valid || !currentParsed.valid) return 0;

  for (size_t i = 0; i < SEGMENT_COUNT; ++i) {
    if (latestParsed.segments[i] != currentParsed.segments[i]) {
      return latestParsed.segments[i] > currentParsed.segments[i] ? 1 : -1;
    }
  }

  if (currentParsed.prerelease && !latestParsed.prerelease) return 1;
  return 0;
}

bool isFirmwareAssetFor(const char* assetName, const char* deviceType) {
  if (assetName == nullptr || deviceType == nullptr || deviceType[0] == '\0') return false;

  constexpr char prefix[] = "firmware-";
  constexpr size_t prefixLength = sizeof(prefix) - 1;
  constexpr char suffix[] = ".bin";
  constexpr size_t suffixLength = sizeof(suffix) - 1;

  if (strncmp(assetName, prefix, prefixLength) != 0) return false;
  const char* rest = assetName + prefixLength;
  const size_t typeLength = strlen(deviceType);
  if (strncmp(rest, deviceType, typeLength) != 0) return false;
  rest += typeLength;

  if (strcmp(rest, suffix) == 0) return true;
  if (rest[0] != '-' || rest[1] != 'v' || !isDigit(rest[2])) return false;
  const size_t restLength = strlen(rest);
  return restLength > suffixLength && strcmp(rest + restLength - suffixLength, suffix) == 0;
}

}  // namespace ota_version
