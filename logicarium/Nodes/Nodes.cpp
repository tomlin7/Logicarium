#include "Nodes.hpp"

namespace Logicarium {
std::vector<Node *(*)()> availableNodes{
    []() -> Node * { return new PinIn(); },
    []() -> Node * { return new PinOut(); }};
} // namespace Logicarium
