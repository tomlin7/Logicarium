#pragma once

#include "Gate.hpp"

#include "AND.hpp"
#include "CustomGate.hpp"
#include "NOT.hpp"
#include "PlaceholderGate.hpp"

#include <functional>

namespace Logicarium {
extern std::vector<std::function<Gate *()>> availableGates;
}
