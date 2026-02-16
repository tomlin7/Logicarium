#include "Gates.hpp"

namespace Logicarium {
std::vector<std::function<Gate *()>> availableGates{
    []() -> Gate * { return new AND(); },
    []() -> Gate * { return new NOT(); },
};
} // namespace Logicarium
