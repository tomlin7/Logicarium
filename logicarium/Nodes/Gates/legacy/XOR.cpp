#include "XOR.hpp"

namespace Logicarium {
XOR::XOR() : Gate("XOR", {{"a"}, {"b"}}, {{"out"}}) {}

bool XOR::XOR_F(const std::vector<bool> &input, const int &) {
  bool current = false;
  for (const bool &pin : input)
    current ^= pin;

  return current;
}

bool XOR::Evaluate() {
  std::vector<bool> input;
  for (const auto &cn : connections)
    if (cn.inputNode == this)
      input.push_back(((Node *)cn.outputNode)->Evaluate());

  value = XOR_F(input, inputSlotCount);
  return value;
}
} // namespace Logicarium
