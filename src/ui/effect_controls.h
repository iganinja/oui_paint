#pragma once

#include "effects/plugins.h"

#include <oui/grid.h>
#include <oui/number_field.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace oui_paint
{

// Puts in `form`, a grid of three columns, a row for each of the effect's parameters: its name, then a slider and a
// field for a number or a list for a choice, set from `values`, which each change updates before `changed` hears
// of it. `values` must outlive the controls. Gives the fields, whose typing is committed by NumberField::commit.
std::vector<olagarro::oui::NumberField*> add_effect_controls(olagarro::oui::Grid& form, const Effect& effect,
                                                             std::map<std::string, double>& values,
                                                             std::function<void()> changed);

} // namespace oui_paint
