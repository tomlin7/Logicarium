#include "PlaceholderGate.hpp"
#include <ImNodes.h>
#include <cstdlib>
#include <imgui.h>


namespace Logicarium {

extern Node *nodeToDuplicate;
extern Node *nodeToDelete;
extern bool nodeHoveredForContextMenu;

PlaceholderGate::PlaceholderGate(const std::string &typeName, int inputs,
                                 int outputs)
    : Gate(typeName.c_str(), {}, {}), missingTypeName(typeName) {

  // Store the original type name as title (will be displayed with "?" prefix)
  title = _strdup(typeName.c_str());

  // Setup slots based on provided counts
  inputSlotCount = inputs;
  outputSlotCount = outputs;

  inputSlots.resize(inputSlotCount);
  outputSlots.resize(outputSlotCount);

  for (int i = 0; i < inputSlotCount; ++i) {
    char buf[16];
    if (inputSlotCount == 1)
      sprintf(buf, "in");
    else
      sprintf(buf, "in%d", i);
    inputSlots[i] = {strdup(buf), 1};
  }

  for (int i = 0; i < outputSlotCount; ++i) {
    char buf[16];
    if (outputSlotCount == 1)
      sprintf(buf, "out");
    else
      sprintf(buf, "out%d", i);
    outputSlots[i] = {strdup(buf), 1};
  }
}

PlaceholderGate::~PlaceholderGate() {
  // Free strdup'd slot names
  for (auto &slot : inputSlots) {
    if (slot.title)
      free((void *)slot.title);
  }
  for (auto &slot : outputSlots) {
    if (slot.title)
      free((void *)slot.title);
  }
  // Free strdup'd title
  if (title)
    free((void *)title);
}

bool PlaceholderGate::Evaluate(const std::string &slot) {
  // Placeholder gates always return false (safe default)
  value = false;
  return false;
}

ImU32 PlaceholderGate::GetColor() const {
  // Warning red color
  return IM_COL32(120, 40, 40, 255);
}

void PlaceholderGate::Render() {
  ImU32 color = GetColor();
  ImU32 borderColor = IM_COL32(200, 80, 80, 255); // Red border

  if (selected) {
    borderColor = IM_COL32(0, 200, 255, 255); // Cyan when selected
  }

  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeTitleBarBg, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeTitleBarBgHovered, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeTitleBarBgActive, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBodyBg, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBodyBgHovered, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBodyBgActive, color);
  ImNodes::Ez::PushStyleColor(ImNodesStyleCol_NodeBorder, borderColor);

  // Display with "?" prefix to indicate missing
  std::string displayTitle = "? " + missingTypeName;

  bool open =
      ImNodes::Ez::BeginNode(this, displayTitle.c_str(), &pos, &selected);
  if (open) {
    ImNodes::Ez::InputSlots(inputSlots.data(), (int)inputSlots.size());
    ImNodes::Ez::OutputSlots(outputSlots.data(), (int)outputSlots.size());

    // Handle new connections (same as Gate::Render)
    void *inNode, *outNode;
    const char *inSlot, *outSlot;
    if (ImNodes::GetNewConnection(&inNode, &inSlot, &outNode, &outSlot)) {
      Connection new_connection;
      new_connection.inputNode = inNode;
      new_connection.inputSlot = inSlot;
      new_connection.outputNode = outNode;
      new_connection.outputSlot = outSlot;

      Node *inputNode = (Node *)new_connection.inputNode;
      Connection existingConnection = {};
      bool foundExisting = false;

      for (const auto &conn : inputNode->connections) {
        if (conn.inputNode == inputNode &&
            conn.inputSlot == new_connection.inputSlot) {
          existingConnection = conn;
          foundExisting = true;
          break;
        }
      }

      if (foundExisting) {
        ((Node *)existingConnection.outputNode)
            ->DeleteConnection(existingConnection);
        inputNode->DeleteConnection(existingConnection);
      }

      ((Node *)new_connection.inputNode)->connections.push_back(new_connection);
      ((Node *)new_connection.outputNode)
          ->connections.push_back(new_connection);
    }

    // Render connections (grayed out since gate doesn't work)
    for (const Connection &connection : connections) {
      if (connection.outputNode != this)
        continue;

      auto *canvas = ImNodes::GetCurrentCanvas();
      ImColor originalColor = canvas->Colors[ImNodes::ColConnection];

      // Gray/muted connection color to indicate inactive
      bool bothSelected = selected && ((Node *)connection.inputNode)->selected;
      if (bothSelected) {
        canvas->Colors[ImNodes::ColConnection] = IM_COL32(0, 200, 255, 255);
      } else {
        canvas->Colors[ImNodes::ColConnection] = IM_COL32(100, 80, 80, 180);
      }

      if (!ImNodes::Connection(
              connection.inputNode, connection.inputSlot.c_str(),
              connection.outputNode, connection.outputSlot.c_str())) {
        ((Node *)connection.inputNode)->DeleteConnection(connection);
        ((Node *)connection.outputNode)->DeleteConnection(connection);
      }

      canvas->Colors[ImNodes::ColConnection] = originalColor;
    }
  }

  ImNodes::Ez::EndNode();
  ImNodes::Ez::PopStyleColor(7);

  // Tooltip on hover
  if (ImGui::IsItemHovered()) {
    nodeHoveredForContextMenu = true;
    ImGui::BeginTooltip();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Missing Gate: %s",
                       missingTypeName.c_str());
    ImGui::Text("Load the custom gate library to enable this gate.");
    ImGui::EndTooltip();
  }

  // Simplified context menu (no Edit option since we can't edit a missing gate)
  if (ImGui::BeginPopupContextItem()) {
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

} // namespace Logicarium
