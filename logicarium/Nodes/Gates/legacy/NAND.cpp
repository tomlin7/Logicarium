#include "NAND.hpp"

namespace Logicarium {
NAND::NAND() : Gate("NAND", {{"a"}, {"b"}}, {{"out"}}) {}

bool NAND::NAND_F(const std::vector<bool> &input, const int &pinCount) {
  if (input.empty())
    return false;
  return !AND::AND_F(input, pinCount);
}

bool NAND::Evaluate() {
  std::vector<bool> input;
  for (const auto &cn : connections)
    if (cn.inputNode == this)
      input.push_back(((Node *)cn.outputNode)->Evaluate());

  value = NAND_F(input, inputSlotCount);
  return value;
}
} // namespace Logicarium
