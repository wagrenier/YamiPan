// The debug menu's teleport: every stage, film and event of the game in one window (F2).
#pragma once

#include <memory>

#include "yamipan/ui/overlay.h"

namespace yamipan::ui
{

std::unique_ptr<Panel> createTeleportPanel();

} // namespace yamipan::ui
