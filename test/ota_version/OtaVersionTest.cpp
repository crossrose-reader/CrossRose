#include <gtest/gtest.h>

#include "OtaVersion.h"

// The device strings below are the exact formats scripts/git_branch.py
// produces (scripts/test_build_identity.py pins that side). The "latest"
// strings are GitHub tag names, which the Release workflow keeps to vX.Y.Z.

TEST(OtaVersion, StableBuildIsNotOfferedItsOwnRelease) {
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-sticky"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-x4-pro"), 0);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-x4-classic"), 0);
}

TEST(OtaVersion, NightlyIsOfferedTheStableReleaseWithItsNumber) {
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-nightly.20261201"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-nightly.20261201-x4-pro"), 1);
  EXPECT_EQ(ota_version::compare("v0.1.0", "0.1.0-nightly.20261201-x4-classic"), 1);
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
