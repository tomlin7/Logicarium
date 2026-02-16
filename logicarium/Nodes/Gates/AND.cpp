#include "AND.hpp"
#include <iostream>

namespace Logicarium {
AND::AND() : Gate("AND", {{"in0"}, {"in1"}}, {{"out"}}) {
  logicCode = "in0 && in1";
}

bool AND::AND_F(const std::vector<bool> &input, const int &pinCount) {
  if (input.size() < pinCount)
    return false;

  bool current = input[0];
  for (const bool &pin : input)
    current &= pin;

  return current;
}

bool AND::Evaluate(const std::string &slot) {
  if (isEvaluating || lastEvaluatedFrame == GlobalFrameCount)
    return value;

  isEvaluating = true;

  // Try dynamic evaluation first
  if (!logicCode.empty()) {
    value = EvaluateExpression();
  } else {
    std::vector<bool> input;
    for (const auto &cn : connections)
      if (cn.inputNode == this)
        input.push_back(((Node *)cn.outputNode)->Evaluate(cn.outputSlot));

    value = AND_F(input, inputSlotCount);
  }

  lastEvaluatedFrame = GlobalFrameCount;
  isEvaluating = false;
  return value;
}
} // namespace Logicarium
