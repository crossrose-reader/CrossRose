#pragma once

#include <I18nKeys.h>

#include <iterator>

#include "CrossPointSettings.h"

// Progress Bar Thickness labels in stored-value order, shared by the device picker and
// the web settings API so a new thickness cannot be missing from either list.
inline constexpr StrId PROGRESS_BAR_THICKNESS_NAMES[] = {
    StrId::STR_PROGRESS_BAR_THIN, StrId::STR_PROGRESS_BAR_MEDIUM, StrId::STR_PROGRESS_BAR_THICK,
    StrId::STR_PROGRESS_BAR_EXTRA_THICK, StrId::STR_PROGRESS_BAR_THICKEST};
static_assert(std::size(PROGRESS_BAR_THICKNESS_NAMES) == CrossPointSettings::STATUS_BAR_PROGRESS_BAR_THICKNESS_COUNT,
              "Every Progress Bar Thickness choice needs a label");
