#pragma once

#include <cstdint>

#include "activities/Activity.h"

// Final screen for Turn Off and Restart. It draws once in onEnter() and keeps
// the default empty render(), so nothing repaints over it while the activities
// below save their state and the device powers down or reboots.
class PowerScreenActivity final : public Activity {
 public:
  enum class Mode : uint8_t { TurnOff, Restart };

  explicit PowerScreenActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const Mode mode)
      : Activity("PowerScreen", renderer, mappedInput), mode(mode) {}
  void onEnter() override;

 private:
  Mode mode;
};
