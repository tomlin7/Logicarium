#include "NOT.hpp"

namespace Logicarium {
NOT::NOT() : Gate("NOT", {{"in"}}, {{"out"}}) { logicCode = "!in"; }

bool NOT::NOT_F(const std::vector<bool> &input, const int &) {
  if (input.empty())
    return true; // Not in -> True? Default behavior of NOT is usually invert
                 // source.

  return !input[0];
}

bool NOT::Evaluate(const std::string &slot) {
  if (isEvaluating || lastEvaluatedFrame == GlobalFrameCount)
    return value;

  isEvaluating = true;

  if (!logicCode.empty()) {
    value = EvaluateExpression();
  } else {
    std::vector<bool> input;
    for (const auto &cn : connections)
      if (cn.inputNode == this)
        input.push_back(((Node *)cn.outputNode)->Evaluate(cn.outputSlot));

    value = NOT_F(input, inputSlotCount);
  }

  lastEvaluatedFrame = GlobalFrameCount;
  isEvaluating = false;
  return value;
}
} // namespace Logicarium
