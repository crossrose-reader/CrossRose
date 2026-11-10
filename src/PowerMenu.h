#pragma once

class OptionPopup;

// The Turn Off / Restart chooser shared by the top drop-down panel's power icon
// and the Home menu's Power entry, so both look and behave the same.
namespace PowerMenu {
// Opens the chooser in `popup` (title "Power", Turn Off selected). Picking an
// option queues the power action with requestPowerAction(); the main loop runs
// it next iteration, after the caller's loop() has returned. Cancelling only
// closes the popup.
void show(OptionPopup& popup);
}  // namespace PowerMenu
