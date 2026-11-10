#include "PowerMenu.h"

#include <I18n.h>

#include <array>

#include "GlobalActions.h"
#include "components/OptionPopup.h"

namespace PowerMenu {

void show(OptionPopup& popup) {
  // Labels stay in flash; show() copies the two translated strings.
  static constexpr std::array<StrId, 2> OPTIONS = {StrId::STR_TURN_OFF, StrId::STR_RESTART};
  OptionPopup* const target = &popup;
  popup.show(StrId::STR_POWER, OPTIONS.data(), static_cast<int>(OPTIONS.size()), 0, [target](const int index) {
    // The power screen replaces everything on the next loop, so a repaint of
    // the screen underneath would only delay it.
    target->skipPostSelectionUpdate();
    requestPowerAction(index == 0 ? PowerAction::TurnOff : PowerAction::Restart);
  });
}

}  // namespace PowerMenu
