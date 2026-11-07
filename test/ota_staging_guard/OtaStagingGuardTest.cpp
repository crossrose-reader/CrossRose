#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string sourceFile(const char* path) {
  std::ifstream source(path);
  return {std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
}

size_t countOf(const std::string& text, const char* needle) {
  size_t count = 0;
  for (size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1)) ++count;
  return count;
}

TEST(OtaStagingGuard, ValidatesBoardTaggedImageBeforeFlashCanStart) {
  const std::string source = sourceFile(OTA_UPDATER_SOURCE_PATH);
  const std::string flasher = sourceFile(FIRMWARE_FLASHER_SOURCE_PATH);
  ASSERT_FALSE(source.empty());
  ASSERT_FALSE(flasher.empty());

  // The old streaming path could start esp_ota before a board tag in a later
  // .rodata chunk was found. Keep direct OTA writes out of OtaUpdater.
  EXPECT_EQ(source.find("esp_ota_begin("), std::string::npos);
  EXPECT_EQ(source.find("esp_ota_write("), std::string::npos);

  const size_t validation = source.find("firmware_flash::validateOpenImageFile(stagedFile");
  const size_t rejection = source.find("validationResult == firmware_flash::Result::BAD_CHIP ||");
  const size_t flash = source.find("firmware_flash::flashValidatedFile(");
  ASSERT_NE(validation, std::string::npos);
  ASSERT_NE(rejection, std::string::npos);
  ASSERT_NE(flash, std::string::npos);
  EXPECT_LT(validation, rejection);
  EXPECT_LT(rejection, flash);

  // The shared open-file path must not reopen by pathname after validation;
  // otherwise a staged file could be replaced before its first erase/write.
  const size_t flashValidated = flasher.find("Result flashValidatedFile(HalFile& file");
  const size_t pathFlasher = flasher.find("Result flashFromSdPath(");
  ASSERT_NE(flashValidated, std::string::npos);
  ASSERT_NE(pathFlasher, std::string::npos);
  EXPECT_EQ(flasher.substr(flashValidated, pathFlasher - flashValidated).find("Storage.openFileForRead"),
            std::string::npos);

  const std::string sharedPathFlasher = flasher.substr(pathFlasher);
  const size_t sharedValidation = sharedPathFlasher.find("validateOpenImageFile(file");
  const size_t sharedFlash = sharedPathFlasher.find("flashValidatedFile(file");
  ASSERT_NE(sharedValidation, std::string::npos);
  ASSERT_NE(sharedFlash, std::string::npos);
  EXPECT_LT(sharedValidation, sharedFlash);
}

// The Boox Picco keeps its vendor "safeboot" app and has one app slot (ota_0).
// There, esp_ota_get_next_update_partition() returns the slot that is running,
// so an update from the reader would erase the firmware it runs from. CrossInk
// 2.0 guards this in several places; keep every one of them.
TEST(OtaStagingGuard, NeverWritesTheRunningAppSlot) {
  const std::string updater = sourceFile(OTA_UPDATER_SOURCE_PATH);
  const std::string flasher = sourceFile(FIRMWARE_FLASHER_SOURCE_PATH);
  const std::string settings = sourceFile(SETTINGS_LIST_SOURCE_PATH);
  const std::string boot = sourceFile(MAIN_SOURCE_PATH);
  ASSERT_FALSE(updater.empty());
  ASSERT_FALSE(flasher.empty());
  ASSERT_FALSE(settings.empty());
  ASSERT_FALSE(boot.empty());

  // Check for Updates refuses the running slot before it downloads anything.
  const size_t pick = updater.find("esp_ota_get_next_update_partition(nullptr)");
  const size_t refuse = updater.find("updatePartition == esp_ota_get_running_partition()");
  const size_t download = updater.find("HttpDownloader::downloadToFile(");
  ASSERT_NE(pick, std::string::npos);
  ASSERT_NE(refuse, std::string::npos);
  ASSERT_NE(download, std::string::npos);
  EXPECT_LT(pick, refuse);
  EXPECT_LT(refuse, download);
  EXPECT_EQ(countOf(updater, "esp_ota_get_next_update_partition("), 1u);

  // Both flasher entry points (OTA staging and SD card) refuse it too.
  EXPECT_EQ(countOf(flasher, "esp_ota_get_next_update_partition("), 2u);
  EXPECT_EQ(countOf(flasher, "dest == esp_ota_get_running_partition()"), 2u);

  // The Picco does not offer the menu items or the SD recovery chord.
  const size_t gate = settings.find("#if !FREEINK_DEVICE_PICCO");
  const size_t check = settings.find("StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates");
  const size_t sd = settings.find("StrId::STR_SD_FIRMWARE_UPDATE, SettingAction::SdFirmwareUpdate");
  ASSERT_NE(gate, std::string::npos);
  ASSERT_NE(check, std::string::npos);
  ASSERT_NE(sd, std::string::npos);
  const size_t gateEnd = settings.find("#endif", gate);
  EXPECT_LT(gate, check);
  EXPECT_LT(gate, sd);
  EXPECT_LT(check, gateEnd);
  EXPECT_LT(sd, gateEnd);
  EXPECT_EQ(countOf(settings, "SettingAction::CheckForUpdates"), 1u);
  EXPECT_EQ(countOf(settings, "SettingAction::SdFirmwareUpdate"), 1u);
  EXPECT_NE(boot.find("!FREEINK_DEVICE_PICCO && mappedInputManager.isPressed(recoveryButton)"), std::string::npos);
}

}  // namespace
