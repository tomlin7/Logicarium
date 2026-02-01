#include "CustomGate.hpp"
#include "../Special/PinIn.hpp"
#include "../Special/PinOut.hpp"
#include "AND.hpp"
#include "NOT.hpp"
#include "PlaceholderGate.hpp"
#include <cstdlib>

namespace Billyprints {

std::map<std::string, GateDefinition> CustomGate::GateRegistry;

Node *CreateNodeByType(const std::string &type) {
  if (type == "AND")
    return new AND();
  if (type == "NOT")
    return new NOT();
  if (type == "In")
    return new PinIn();
  if (type == "Out")
    return new PinOut();

  // Check Custom Gate Registry
  if (CustomGate::GateRegistry.count(type)) {
    return new CustomGate(CustomGate::GateRegistry[type]);
  }

  return nullptr;
}

Node *CreateNodeByTypeOrPlaceholder(const std::string &type, int inputHint,
                                    int outputHint) {
  // Try to create the real node first
  Node *node = CreateNodeByType(type);
  if (node)
    return node;

  // If type not found, create a placeholder
  return new PlaceholderGate(type, inputHint, outputHint);
}

CustomGate::CustomGate(const GateDefinition &def)
    : Gate(def.name.c_str(), {}, {}), definition(def) {
  title = _strdup(def.name.c_str()); // ImNodes needs a char*

  // 1. Create Internal Nodes
  std::map<int, Node *> nodeMap; // Map definition ID to actual Node*

  for (const auto &nodeDef : def.nodes) {
    Node *newNode = CreateNodeByType(nodeDef.type);
    if (newNode) {
      // newNode->pos = nodeDef.pos; // Position doesn't matter for logic, only
      // for editing if we allowed opening it
      internalNodes.push_back(newNode);
      nodeMap[nodeDef.id] = newNode;

      if (nodeDef.type == "In") {
        internalInputs.push_back((PinIn *)newNode);
      } else if (nodeDef.type == "Out") {
        internalOutputs.push_back((PinOut *)newNode);
      }
    }
  }

  // 2. Setup External Slots based on PinIn/PinOut counts
  // Sort internalInputs/Outputs based on some logic (e.g., Y position) if
  // needed, but for now relying on definition order (which should be sorted by
  // creation or position)

  inputSlotCount = (int)internalInputs.size();
  outputSlotCount = (int)internalOutputs.size();

  inputSlots.resize(inputSlotCount);
  outputSlots.resize(outputSlotCount);

  for (int i = 0; i < inputSlotCount; ++i) {
    // Use custom pin names if defined (from script), otherwise indexed names
    if (i < (int)def.inputPinNames.size() && !def.inputPinNames[i].empty()) {
      inputSlots[i] = {strdup(def.inputPinNames[i].c_str()), 1};
    } else {
      char buf[16];
      if (inputSlotCount == 1)
        sprintf(buf, "in");
      else
        sprintf(buf, "in%d", i);
      inputSlots[i] = {strdup(buf), 1};
    }
  }
  for (int i = 0; i < outputSlotCount; ++i) {
    // Use custom pin names if defined (from script), otherwise indexed names
    if (i < (int)def.outputPinNames.size() && !def.outputPinNames[i].empty()) {
      outputSlots[i] = {strdup(def.outputPinNames[i].c_str()), 1};
    } else {
      char buf[16];
      if (outputSlotCount == 1)
        sprintf(buf, "out");
      else
        sprintf(buf, "out%d", i);
      outputSlots[i] = {strdup(buf), 1};
    }
  }

  // 3. Create Internal Connections
  for (const auto &connDef : definition.connections) {
    if (nodeMap.count(connDef.inputNodeId) &&
        nodeMap.count(connDef.outputNodeId)) {
      Connection conn;
      conn.inputNode = nodeMap[connDef.inputNodeId];
      conn.inputSlot = connDef.inputSlot.c_str();
      conn.outputNode = nodeMap[connDef.outputNodeId];
      conn.outputSlot = connDef.outputSlot.c_str();

      // Link them virtually
      ((Node *)conn.inputNode)->connections.push_back(conn);
      ((Node *)conn.outputNode)->connections.push_back(conn);

      internalConnections.push_back(conn);
    }
  }
}

CustomGate::~CustomGate() {
  // Delete internal nodes first
  for (auto node : internalNodes) {
    delete node;
  }
  // Free strdup'd slot names
  for (auto &slot : inputSlots) {
    if (slot.title)
      free((void *)slot.title);
  }
  for (auto &slot : outputSlots) {
    if (slot.title)
      free((void *)slot.title);
  }
  // Free strdup'd title last
  if (title)
    free((void *)title);
}

bool CustomGate::Evaluate(const std::string &slot) {
  if (isEvaluating) {
    return outputCache[slot]; // Simple cycle break
  }

  if (lastEvaluatedFrame == Node::GlobalFrameCount) {
    if (slot.empty() && !internalOutputs.empty()) {
      return outputCache[outputSlots[0].title];
    }
    return outputCache[slot];
  }

  isEvaluating = true;

  // Step A: Update Internal PinIns from external connections
  for (int i = 0; i < inputSlots.size(); ++i) {
    bool slotValue = false;
    const char *slotName = inputSlots[i].title;

    for (const auto &conn : connections) {
      if (conn.inputNode == this && !conn.inputSlot.empty() &&
          strcmp(conn.inputSlot.c_str(), slotName) == 0) {
        Node *source = (Node *)conn.outputNode;
        slotValue = source->Evaluate(conn.outputSlot); // Pull from source slot
        break;
      }
    }

    if (i < (int)internalInputs.size()) {
      internalInputs[i]->value = slotValue;
      internalInputs[i]->lastEvaluatedFrame = GlobalFrameCount;
    }
  }

  // Step B: Reset internal nodes' cache for this frame
  // This allows the pull-back recursion to reach through the internal logic
  for (auto node : internalNodes) {
    node->lastEvaluatedFrame = 0;
  }

  // Step C: Trigger recursion from all outputs and cache results
  // For complex circuits with feedback, we might still want multiple passes,
  // but for DAGs, one recursive pull per output is correct.
  for (int i = 0; i < (int)internalOutputs.size(); ++i) {
    bool val = internalOutputs[i]->Evaluate(); // Pulls through internal logic
    outputCache[outputSlots[i].title] = val;
  }

  // Handle empty name or default
  if (!internalOutputs.empty()) {
    value = outputCache[outputSlots[0].title];
  } else {
    value = false;
  }

  lastEvaluatedFrame = Node::GlobalFrameCount;
  isEvaluating = false;

  if (slot.empty())
    return value;
  return outputCache[slot];
}

} // namespace Billyprints
