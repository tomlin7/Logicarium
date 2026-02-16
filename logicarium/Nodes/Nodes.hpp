#pragma once

#include "Node.hpp"

#include "AND.hpp"
#include "NOT.hpp"
#include "PinIn.hpp"
#include "PinOut.hpp"


namespace Logicarium {
extern std::vector<Node *(*)()> availableNodes;
}
