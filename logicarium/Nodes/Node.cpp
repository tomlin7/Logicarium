#include "Node.hpp"

namespace Logicarium {
uint64_t Node::GlobalFrameCount = 0;
Node::Node(const char *_title, std::vector<ImNodes::Ez::SlotInfo> &&_inputSlots,
           std::vector<ImNodes::Ez::SlotInfo> &&_outputSlots) {
  title = _title;
  inputSlots = _inputSlots;
  outputSlots = _outputSlots;

  inputSlotCount = static_cast<int>(inputSlots.size());
  outputSlotCount = static_cast<int>(outputSlots.size());
}

void Node::DeleteConnection(const Connection &connection) {
  for (auto it = connections.begin(); it != connections.end(); ++it) {
    if (connection == *it) {
      connections.erase(it);
      break;
    }
  }
}

bool Node::Evaluate(const std::string &slot) {
  if (isEvaluating || lastEvaluatedFrame == GlobalFrameCount)
    return value;
  return value;
}

ImU32 Node::GetColor() const { return IM_COL32(40, 40, 45, 255); }

void Node::Render() {
  // Default empty render - individual types should override
}
} // namespace Logicarium
