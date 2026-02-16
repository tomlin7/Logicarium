#include "Buffer.hpp"

namespace Logicarium {
Buffer::Buffer() : Gate("Buffer", {{"in"}}, {{"out"}}) {}

bool Buffer::Buffer_F(const std::vector<bool> &input, const int &) {
  for (const bool &pin : input)
    if (pin)
      return pin;

  return false;
}

bool Buffer::Evaluate() {
  std::vector<bool> input;
  for (const auto &cn : connections)
    if (cn.inputNode == this)
      input.push_back(((Node *)cn.outputNode)->Evaluate());

  value = Buffer_F(input, inputSlotCount);
  return value;
}
} // namespace Logicarium
