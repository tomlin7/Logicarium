#include "NOR.hpp"

namespace Logicarium {
NOR::NOR() : Gate("NOR", {{"a"}, {"b"}}, {{"out"}}) {}

bool NOR::NOR_F(const std::vector<bool> &input, const int &pinCount) {
  return !OR::OR_F(input, pinCount);
}

bool NOR::Evaluate() {
  std::vector<bool> input;
  for (const auto &cn : connections)
    if (cn.inputNode == this)
      input.push_back(((Node *)cn.outputNode)->Evaluate());

  value = NOR_F(input, inputSlotCount);
  return value;
}
} // namespace Logicarium
