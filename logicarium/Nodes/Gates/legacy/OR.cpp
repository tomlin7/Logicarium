#include "OR.hpp"

namespace Logicarium {
OR::OR() : Gate("OR", {{"a"}, {"b"}}, {{"out"}}) {}

bool OR::OR_F(const std::vector<bool> &input, const int &) {
  bool current = false;
  for (const bool &pin : input)
    current |= pin;

  return current;
}

bool OR::Evaluate() {
  std::vector<bool> input;
  for (const auto &cn : connections)
    if (cn.inputNode == this)
      input.push_back(((Node *)cn.outputNode)->Evaluate());

  value = OR_F(input, outputSlotCount);
  return value;
}
} // namespace Logicarium
