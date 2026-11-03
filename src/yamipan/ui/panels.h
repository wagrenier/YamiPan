#pragma once

#include <memory>
#include <vector>

#include "yamipan/ui/overlay.h"

namespace yamipan::ui
{

// The menu bar's content, left to right.
std::vector<std::unique_ptr<Panel>> createPanels(Application &application);

} // namespace yamipan::ui
