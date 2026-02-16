#include "Gate.hpp"
#include "CustomGate.hpp"
#include <string>

namespace Logicarium {
extern Node *nodeToDuplicate;
extern Node *nodeToEdit;
extern Node *nodeToDelete;
extern Node *nodeToSaveGate; // Gate to save permanently
extern bool nodeHoveredForContextMenu;

Gate::Gate(const char *_title, std::vector<ImNodes::Ez::SlotInfo> &&_inputSlots,
           std::vector<ImNodes::Ez::SlotInfo> &&_outputSlots)
    : Node(_title, std::move(_inputSlots), std::move(_outputSlots)) {}

ImU32 Gate::GetColor() const {
  std::string t = title;
  if (t == "AND")
    return IM_COL32(10, 30, 60, 255);
  if (t == "NOT")
    return IM_COL32(80, 20, 20, 255);
  if (t == "NAND")
    return IM_COL32(40, 10, 80, 255);
  return Node::GetColor();
}

void Gate::Render() {
  ImU32 color = GetColor();
  color = (color & 0x00FFFFFF) | 0xFF000000; // Force solid

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

  bool open = ImNodes::Ez::BeginNode(this, title, &pos, &selected);
  if (open) {
    ImNodes::Ez::InputSlots(inputSlots.data(), (int)inputSlots.size());
    ImNodes::Ez::OutputSlots(outputSlots.data(), (int)outputSlots.size());

    // Handle new connections
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

    // Render output connections
    for (const Connection &connection : connections) {
      if (connection.outputNode != this)
        continue;

      bool signal = Evaluate(connection.outputSlot);
      ImColor activeColor = IM_COL32(50, 255, 150, 255);
      ImColor inactiveColor = IM_COL32(80, 90, 100, 255);

      auto *canvas = ImNodes::GetCurrentCanvas();
      ImColor originalConnectionColor = canvas->Colors[ImNodes::ColConnection];

      // Check if both nodes are selected for connection highlighting
      bool bothSelected = selected && ((Node *)connection.inputNode)->selected;
      if (bothSelected) {
        canvas->Colors[ImNodes::ColConnection] = IM_COL32(0, 200, 255, 255);
      } else {
        canvas->Colors[ImNodes::ColConnection] =
            signal ? activeColor : inactiveColor;
      }

      if (!ImNodes::Connection(
              connection.inputNode, connection.inputSlot.c_str(),
              connection.outputNode, connection.outputSlot.c_str())) {
        ((Node *)connection.inputNode)->DeleteConnection(connection);
        ((Node *)connection.outputNode)->DeleteConnection(connection);
      }
      canvas->Colors[ImNodes::ColConnection] = originalConnectionColor;
    }
  }

  ImNodes::Ez::EndNode();
  ImNodes::Ez::PopStyleColor(7);

  if (ImGui::IsItemHovered()) {
    nodeHoveredForContextMenu = true;
  }

  if (ImGui::BeginPopupContextItem()) {
    if (ImGui::MenuItem("Duplicate")) {
      nodeToDuplicate = this;
    }
    if (std::string(title) != "In" && std::string(title) != "Out") {
      bool isCustom = CustomGate::GateRegistry.count(title);
      bool isTemporary =
          isCustom && CustomGate::GateRegistry[title].isTemporary;

      if (isCustom && isTemporary) {
        // Show "Save Gate" for temporary (script-defined) custom gates
        if (ImGui::MenuItem("Save Gate")) {
          nodeToSaveGate = this;
        }
      }

      if (!isTemporary) {
        // Only show Edit Circuit/Logic for saved gates
        if (ImGui::MenuItem(isCustom ? "Edit Circuit" : "Edit Logic")) {
          nodeToEdit = this;
        }
      }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete", "Del")) {
      nodeToDelete = this;
    }
    ImGui::EndPopup();
  }
}
bool Gate::EvaluateExpression() {
  if (logicCode.empty())
    return false;

  // Very basic expression evaluator for Boolean logic
  // Supports: !, &&, ||, ^, (, ), in0, in1, ...

  // 1. Get current input values
  std::map<std::string, bool> inputs;
  for (int i = 0; i < inputSlotCount; ++i) {
    std::string name = inputSlots[i].title;

    bool val = false;
    for (const auto &conn : connections) {
      if (conn.inputNode == this && conn.inputSlot == name) {
        val = ((Node *)conn.outputNode)->Evaluate(conn.outputSlot);
        break;
      }
    }
    inputs[name] = val;
  }

  // 2. Simple recursive descent if possible, or just a basic replace-and-eval
  // For now, let's just implement a tiny tokenizer and stack-based eval
  std::string expr = logicCode;

  // Replace tokens with values
  auto replaceAll = [&](std::string &s, const std::string &from,
                        const std::string &to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
      s.replace(pos, from.length(), to);
      pos += to.length();
    }
  };

  for (auto const &[name, val] : inputs) {
    replaceAll(expr, name, val ? "1" : "0");
  }

  // Basic evaluation of the resulting string (0s and 1s with !, &&, ||)
  // This is a placeholder for a better parser.
  // For now, let's just do a tiny one.

  auto eval = [&](auto self, std::string s) -> bool {
    while (s.find(' ') != std::string::npos)
      s.erase(s.find(' '), 1);
    if (s.empty())
      return false;
    if (s == "1")
      return true;
    if (s == "0")
      return false;

    // Simple operators
    if (s[0] == '!')
      return !self(self, s.substr(1));

    size_t pos;
    if ((pos = s.find("||")) != std::string::npos)
      return self(self, s.substr(0, pos)) || self(self, s.substr(pos + 2));
    if ((pos = s.find("&&")) != std::string::npos)
      return self(self, s.substr(0, pos)) && self(self, s.substr(pos + 2));

    return false;
  };

  try {
    return eval(eval, expr);
  } catch (...) {
    return false;
  }
}
} // namespace Logicarium
