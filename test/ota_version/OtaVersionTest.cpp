#include <gtest/gtest.h>

#include <string>

#include "OtaVersion.h"

// The device strings below are the exact formats scripts/git_branch.py
// produces (scripts/test_build_identity.py pins that side). The "latest"
// strings are GitHub tag names, which the Release workflow keeps to vX.Y.Z.

TEST(OtaVersion, StableBuildIsNotOfferedItsOwnRelease) {
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-sticky"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-x4-pro"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-x4-classic"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-picco"), 0);
}

TEST(OtaVersion, NightlyIsOfferedTheStableReleaseWithItsNumber) {
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-nightly.20261201"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-nightly.20261201-x4-pro"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-nightly.20261201-x4-classic"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-nightly.20261201-picco"), 1);
}

TEST(OtaVersion, NightlyOfTheNextVersionIsNotOfferedAnOlderRelease) {
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.2.0-nightly.20261201"), -1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.2.0-nightly.20261201-sticky"), -1);
}

TEST(OtaVersion, BuildsFromSourceAreOfferedTheStableReleaseWithTheirNumber) {
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-dev+main"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-dev+fix-rcfoo-x4-pro"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-main+a1b2c"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-detached+a1b2c"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.2.0-dev+main-sticky"), -1);
}

TEST(OtaVersion, ReleaseCandidatesArePrereleases) {
  EXPECT_EQ(ota_version::compare("v0.2.0", "0.2.0-rc1"), 1);
  EXPECT_EQ(ota_version::compare("v1.6.1", "1.6.1-RC"), 1);
}

TEST(OtaVersion, ComparesEachNumberAsANumber) {
  EXPECT_EQ(ota_version::compare("v0.10.0", "0.9.0"), 1);
  EXPECT_EQ(ota_version::compare("v0.9.0", "0.10.0"), -1);
  EXPECT_EQ(ota_version::compare("v1.0.0", "0.10.0-x4-pro"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.1", "0.1.0-x4-classic"), 1);
  EXPECT_EQ(ota_version::compare("v1.4.0.1", "1.4.0"), 1);
  EXPECT_EQ(ota_version::compare("V0.2.0", "0.1.0"), 1);
}

TEST(OtaVersion, OlderLinePatchIsNotOfferedToNewerDevices) {
  // A fix for an older line published as "latest" by mistake.
  EXPECT_EQ(ota_version::compare("v0.1.1", "0.2.0"), -1);
  EXPECT_EQ(ota_version::compare("v0.1.1", "0.2.0-x4-pro"), -1);
}

TEST(OtaVersion, UnparsableVersionsAreNeverNewer) {
  EXPECT_EQ(ota_version::compare("v0.1.0", "dev"), 0);
  EXPECT_EQ(ota_version::compare("nightly", "0.1.0"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", nullptr), 0);
  EXPECT_EQ(ota_version::compare("v0.1.", "0.1.0"), 0);
}

TEST(OtaVersion, MarksWhichBuildsMayRunBeforeAnyStableRelease) {
  // OtaUpdater treats GitHub's 404 ("no published stable release") as
  // "no update" only for these.
  EXPECT_FALSE(ota_version::parse("dev").valid);
  EXPECT_TRUE(ota_version::parse("0.1.0-nightly.20261201-x4-pro").prerelease);
  EXPECT_TRUE(ota_version::parse("0.1.0-dev+main").prerelease);
  EXPECT_TRUE(ota_version::parse("0.1.0-main+a1b2c").prerelease);

  const ota_version::ParsedVersion stable = ota_version::parse("0.1.0-x4-classic");
  EXPECT_TRUE(stable.valid);
  EXPECT_FALSE(stable.prerelease);
  EXPECT_EQ(stable.segments[0], 0);
  EXPECT_EQ(stable.segments[1], 1);
  EXPECT_EQ(stable.segments[2], 0);
}

// Device types as in platformio.ini (CROSSINK_FIRMWARE_DEVICE_TYPE) and the
// file names .github/workflows/build-firmware.yml uploads to a release.
namespace {
constexpr const char* kDeviceTypes[] = {"x3-x4", "sticky", "x4-pro", "x4-classic", "picco"};
}

TEST(OtaVersion, EachDeviceTakesOnlyItsOwnFirmwareFile) {
  for (const char* owner : kDeviceTypes) {
    const std::string stable = std::string("firmware-") + owner + "-v0.1.0.bin";
    const std::string nightly = std::string("firmware-") + owner + "-v0.1.0-nightly.20261201.bin";
    const std::string plain = std::string("firmware-") + owner + ".bin";
    for (const char* device : kDeviceTypes) {
      const bool same = std::string(owner) == device;
      EXPECT_EQ(ota_version::isFirmwareAssetFor(stable.c_str(), device), same) << stable << " on " << device;
      EXPECT_EQ(ota_version::isFirmwareAssetFor(nightly.c_str(), device), same) << nightly << " on " << device;
      EXPECT_EQ(ota_version::isFirmwareAssetFor(plain.c_str(), device), same) << plain << " on " << device;
    }
  }
}

TEST(OtaVersion, AShorterDeviceTypeDoesNotTakeALongerOnesFile) {
  // No device is called "x4" today; this keeps one from taking X4 Pro files.
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-x4-pro-v0.1.0.bin", "x4"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-x4-classic-v0.1.0.bin", "x4"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-x3-x4-v0.1.0.bin", "x3"));
  EXPECT_TRUE(ota_version::isFirmwareAssetFor("firmware-x4-v0.1.0.bin", "x4"));
}

TEST(OtaVersion, OtherReleaseFilesAreNotFirmware) {
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("crossink-languages-0.1.0.zip", "picco"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("language-template.json", "picco"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-picco-v0.1.0.bin.sha256", "picco"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-picco-v.bin", "picco"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-picco-.bin", "picco"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-picco", "picco"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor(nullptr, "picco"));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-.bin", ""));
  EXPECT_FALSE(ota_version::isFirmwareAssetFor("firmware-picco-v0.1.0.bin", nullptr));
}
