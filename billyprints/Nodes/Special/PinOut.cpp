#include "PinOut.hpp"

namespace Billyprints {
PinOut::PinOut() : Node("Out", {{"in"}}, {}) { value = true; };

bool PinOut::Evaluate(const std::string &slot) {
  if (isEvaluating || lastEvaluatedFrame == GlobalFrameCount)
    return value;

  isEvaluating = true;
  for (const auto &cn : connections) {
    if (cn.inputNode == this &&
        ((Node *)cn.outputNode)->Evaluate(cn.outputSlot)) {
      value = true;
      lastEvaluatedFrame = GlobalFrameCount;
      isEvaluating = false;
      return value;
    }
  }

  value = false;
  lastEvaluatedFrame = GlobalFrameCount;
  isEvaluating = false;
  return value;
};

extern Node *nodeToDuplicate;
extern Node *nodeToDelete;
extern Node *nodeToRename;
extern bool nodeHoveredForContextMenu;

void PinOut::Render() {
  ImU32 color = GetColor();
  color = (color & 0x00FFFFFF) | 0xFF000000;
  ImU32 borderColor =
      Evaluate() ? IM_COL32(50, 255, 150, 255) : IM_COL32(50, 50, 50, 50);

  // Selection highlight - bright cyan border when selected
  if (selected) {
    borderColor = IM_COL32(0, 200, 255, 255);
  }

  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeTitleBarBg, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeTitleBarBgHovered, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeTitleBarBgActive, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBodyBg, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBodyBgHovered, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBodyBgActive, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBorder, borderColor);

  // Display ID if available, otherwise "Out"
  std::string displayTitle = id.empty() ? "Out" : id;

  bool open =
      ImNodes::Ez::BeginNode(this, displayTitle.c_str(), &pos, &selected);
  if (open) {
    ImNodes::Ez::InputSlots(inputSlots.data(), inputSlotCount);

    bool signal = Evaluate();
    ImGui::PushStyleColor(ImGuiCol_Button, signal
                                               ? ImVec4(0, 0.8f, 0, 1)
                                               : ImVec4(0.1f, 0.1f, 0.1f, 1));
    ImGui::Button(signal ? "HIGH" : "LOW", ImVec2(40, 30));
    ImGui::PopStyleColor();

    ImNodes::Ez::OutputSlots(outputSlots.data(), outputSlotCount);
  }

  ImNodes::Ez::EndNode();
  ImNodes::Ez::PopStyleColor(7);

  if (ImGui::IsItemHovered()) {
    nodeHoveredForContextMenu = true;
  }

  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Rename")) {
      nodeToRename = this;
    }
    if (ImGui::MenuItem("Duplicate")) {
      nodeToDuplicate = this;
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete", "Del")) {
      nodeToDelete = this;
    }
    ImGui::EndPopup();
  }
}
} // namespace Billyprints
