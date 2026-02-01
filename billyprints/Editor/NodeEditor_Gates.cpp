#include "../Nodes/Gates/CustomGate.hpp"
#include "../Nodes/Gates/PlaceholderGate.hpp"
#include "NodeEditor.hpp"
#include <ImNodes.h>
#include <algorithm>
#include <functional>
#include <imgui.h>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Billyprints {

// Helper to check if a type is a built-in type (actually created by
// CreateNodeByType)
static bool IsBuiltInType(const std::string &type) {
  // Only types that CreateNodeByType can actually create without the registry
  return type == "AND" || type == "NOT" || type == "In" || type == "Out" ||
         type == "Input" || type == "Output";
}

void NodeEditor::CreateGate() {
  GateDefinition def;
  def.name = std::string(gateName);

  std::map<Node *, int> nodePtrToId;
  int idCounter = 0;

  // 1. Collect Nodes
  for (auto *node : nodes) {
    NodeDefinition nd;
    nd.id = idCounter++;
    nodePtrToId[node] = nd.id;
    nd.pos = node->pos;

    nd.type = node->title;

    def.nodes.push_back(nd);

    std::string titleStr = node->title;
    if (titleStr == "Input" || titleStr == "In")
      def.inputPinIndices.push_back(nd.id);
    else if (titleStr == "Output" || titleStr == "Out")
      def.outputPinIndices.push_back(nd.id);
  }

  // 2. Collect Connections
  for (auto *node : nodes) {
    for (const auto &conn : node->connections) {
      if (conn.outputNode == node) {
        ConnectionDefinition cd;
        cd.inputNodeId = nodePtrToId[(Node *)conn.inputNode];
        cd.inputSlot = conn.inputSlot;
        cd.outputNodeId = nodePtrToId[(Node *)conn.outputNode];
        cd.outputSlot = conn.outputSlot;
        def.connections.push_back(cd);
      }
    }
  }

  // 3. Register
  FILE *f = fopen("debug.txt", "a");
  if (f) {
    fprintf(f, "Registering new gate: %s with %zu nodes and %zu connections\n",
            def.name.c_str(), def.nodes.size(), def.connections.size());
    fclose(f);
  }

  // Store definition for serialization
  def.color =
      IM_COL32((int)(newGateColor[0] * 255), (int)(newGateColor[1] * 255),
               (int)(newGateColor[2] * 255), 200);
  def.isTemporary = false; // UI-created gates are permanent

  customGateDefinitions.push_back(def);
  CustomGate::GateRegistry[def.name] = def;

  availableGates.push_back([def]() -> Gate * { return new CustomGate(def); });
}

void NodeEditor::SaveGates(const std::string &filename) {
  FILE *f = fopen(filename.c_str(), "wb");
  if (!f)
    return;

  size_t count = customGateDefinitions.size();
  fwrite(&count, sizeof(size_t), 1, f);

  for (const auto &def : customGateDefinitions) {
    // Name
    size_t nameLen = def.name.size();
    fwrite(&nameLen, sizeof(size_t), 1, f);
    fwrite(def.name.c_str(), 1, nameLen, f);

    // Color
    fwrite(&def.color, sizeof(ImU32), 1, f);

    // Nodes
    size_t nodeCount = def.nodes.size();
    fwrite(&nodeCount, sizeof(size_t), 1, f);
    for (const auto &node : def.nodes) {
      size_t typeLen = node.type.size();
      fwrite(&typeLen, sizeof(size_t), 1, f);
      fwrite(node.type.c_str(), 1, typeLen, f);
      fwrite(&node.pos, sizeof(ImVec2), 1, f);
      fwrite(&node.id, sizeof(int), 1, f);
    }

    // Connections
    size_t connCount = def.connections.size();
    fwrite(&connCount, sizeof(size_t), 1, f);
    for (const auto &conn : def.connections) {
      fwrite(&conn.inputNodeId, sizeof(int), 1, f);

      size_t inSlotLen = conn.inputSlot.size();
      fwrite(&inSlotLen, sizeof(size_t), 1, f);
      fwrite(conn.inputSlot.c_str(), 1, inSlotLen, f);

      fwrite(&conn.outputNodeId, sizeof(int), 1, f);

      size_t outSlotLen = conn.outputSlot.size();
      fwrite(&outSlotLen, sizeof(size_t), 1, f);
      fwrite(conn.outputSlot.c_str(), 1, outSlotLen, f);
    }

    // Pin Indices
    size_t inPinCount = def.inputPinIndices.size();
    fwrite(&inPinCount, sizeof(size_t), 1, f);
    fwrite(def.inputPinIndices.data(), sizeof(int), inPinCount, f);

    size_t outPinCount = def.outputPinIndices.size();
    fwrite(&outPinCount, sizeof(size_t), 1, f);
    fwrite(def.outputPinIndices.data(), sizeof(int), outPinCount, f);
  }
  fclose(f);
}

void NodeEditor::LoadGates(const std::string &filename) {
  FILE *f = fopen(filename.c_str(), "rb");
  if (!f)
    return;

  customGateDefinitions.clear();
  availableGates = {
      []() -> Gate * { return new AND(); },
      []() -> Gate * { return new NOT(); },
  };

  size_t count = 0;
  fread(&count, sizeof(size_t), 1, f);

  for (size_t i = 0; i < count; i++) {
    GateDefinition def;

    // Name
    size_t nameLen = 0;
    fread(&nameLen, sizeof(size_t), 1, f);
    def.name.resize(nameLen);
    fread(&def.name[0], 1, nameLen, f);

    // Color
    fread(&def.color, sizeof(ImU32), 1, f);

    // Nodes
    size_t nodeCount = 0;
    fread(&nodeCount, sizeof(size_t), 1, f);
    for (size_t j = 0; j < nodeCount; j++) {
      NodeDefinition nd;
      size_t typeLen = 0;
      fread(&typeLen, sizeof(size_t), 1, f);
      nd.type.resize(typeLen);
      fread(&nd.type[0], 1, typeLen, f);
      fread(&nd.pos, sizeof(ImVec2), 1, f);
      fread(&nd.id, sizeof(int), 1, f);
      def.nodes.push_back(nd);
    }

    // Connections
    size_t connCount = 0;
    fread(&connCount, sizeof(size_t), 1, f);
    for (size_t j = 0; j < connCount; j++) {
      ConnectionDefinition cd;
      fread(&cd.inputNodeId, sizeof(int), 1, f);

      size_t inSlotLen = 0;
      fread(&inSlotLen, sizeof(size_t), 1, f);
      cd.inputSlot.resize(inSlotLen);
      fread(&cd.inputSlot[0], 1, inSlotLen, f);

      fread(&cd.outputNodeId, sizeof(int), 1, f);

      size_t outSlotLen = 0;
      fread(&outSlotLen, sizeof(size_t), 1, f);
      cd.outputSlot.resize(outSlotLen);
      fread(&cd.outputSlot[0], 1, outSlotLen, f);

      def.connections.push_back(cd);
    }

    // Pin Indices
    size_t inPinCount = 0;
    fread(&inPinCount, sizeof(size_t), 1, f);
    def.inputPinIndices.resize(inPinCount);
    fread(def.inputPinIndices.data(), sizeof(int), inPinCount, f);

    size_t outPinCount = 0;
    fread(&outPinCount, sizeof(size_t), 1, f);
    def.outputPinIndices.resize(outPinCount);
    fread(def.outputPinIndices.data(), sizeof(int), outPinCount, f);

    def.isTemporary = false; // Loaded gates are permanent
    customGateDefinitions.push_back(def);
    CustomGate::GateRegistry[def.name] = def;
    availableGates.push_back([def]() -> Gate * { return new CustomGate(def); });
  }
  fclose(f);

  // Try to upgrade any placeholder nodes that may now have their definitions
  TryUpgradePlaceholders();
}

void NodeEditor::TryUpgradePlaceholders() {
  if (placeholderNodes.empty())
    return;

  std::vector<PlaceholderGate *> upgraded;

  for (auto *placeholder : placeholderNodes) {
    // Check if the gate definition is now available
    if (CustomGate::GateRegistry.count(placeholder->missingTypeName)) {
      // Create the real gate
      auto *realGate = new CustomGate(
          CustomGate::GateRegistry[placeholder->missingTypeName]);

      // Copy position and ID
      realGate->pos = placeholder->pos;
      realGate->id = placeholder->id;
      realGate->selected = placeholder->selected;

      // Transfer connections
      for (auto &conn : placeholder->connections) {
        // Update the connection's node pointers
        if (conn.inputNode == placeholder) {
          conn.inputNode = realGate;
        }
        if (conn.outputNode == placeholder) {
          conn.outputNode = realGate;
        }

        // Add to real gate's connection list
        realGate->connections.push_back(conn);

        // Update the other node's reference to point to real gate
        Node *otherNode = (conn.inputNode == realGate) ? (Node *)conn.outputNode
                                                       : (Node *)conn.inputNode;

        for (auto &otherConn : otherNode->connections) {
          if (otherConn.inputNode == placeholder) {
            otherConn.inputNode = realGate;
          }
          if (otherConn.outputNode == placeholder) {
            otherConn.outputNode = realGate;
          }
        }
      }

      // Replace in nodes list
      auto it = std::find(nodes.begin(), nodes.end(), (Node *)placeholder);
      if (it != nodes.end()) {
        *it = realGate;
      }

      upgraded.push_back(placeholder);
    }
  }

  // Remove upgraded placeholders from tracking and delete them
  for (auto *p : upgraded) {
    placeholderNodes.erase(p);
    delete p;
  }

  // Update the missing types list and banner state
  if (placeholderNodes.empty()) {
    showMissingGatesBanner = false;
    missingGateTypes.clear();
  } else {
    // Rebuild missing types list from remaining placeholders
    missingGateTypes.clear();
    for (auto *p : placeholderNodes) {
      if (std::find(missingGateTypes.begin(), missingGateTypes.end(),
                    p->missingTypeName) == missingGateTypes.end()) {
        missingGateTypes.push_back(p->missingTypeName);
      }
    }
  }
}

void NodeEditor::SaveScene(const std::string &filename) {
  FILE *f = fopen(filename.c_str(), "wb");
  if (!f)
    return;

  // Build node ID map
  std::map<Node *, int> nodePtrToId;
  int idCounter = 0;

  // Write magic number for scene files (BPS2 format)
  const char magic[4] = {'B', 'P', 'S', '2'}; // Billyprints Scene v2
  fwrite(magic, 1, 4, f);

  // Collect custom gate types used in the scene
  std::set<std::string> customTypesUsed;
  for (auto *node : nodes) {
    std::string type = node->title;
    // For PlaceholderGate, use the original missing type name
    if (auto *placeholder = dynamic_cast<PlaceholderGate *>(node)) {
      type = placeholder->missingTypeName;
    }
    if (!IsBuiltInType(type)) {
      customTypesUsed.insert(type);
    }
  }

  // Write custom gate dependency section
  size_t customTypeCount = customTypesUsed.size();
  fwrite(&customTypeCount, sizeof(size_t), 1, f);
  for (const auto &typeName : customTypesUsed) {
    size_t len = typeName.size();
    fwrite(&len, sizeof(size_t), 1, f);
    fwrite(typeName.c_str(), 1, len, f);
  }

  // Write node count
  size_t nodeCount = nodes.size();
  fwrite(&nodeCount, sizeof(size_t), 1, f);

  // Write nodes
  for (auto *node : nodes) {
    nodePtrToId[node] = idCounter++;

    // Node type (use original type name for placeholders)
    std::string type = node->title;
    if (auto *placeholder = dynamic_cast<PlaceholderGate *>(node)) {
      type = placeholder->missingTypeName;
    }
    size_t typeLen = type.size();
    fwrite(&typeLen, sizeof(size_t), 1, f);
    fwrite(type.c_str(), 1, typeLen, f);

    // Node position
    fwrite(&node->pos, sizeof(ImVec2), 1, f);

    // Slot counts (new in BPS2 - needed for placeholder reconstruction)
    int inputCount = node->inputSlotCount;
    int outputCount = node->outputSlotCount;
    fwrite(&inputCount, sizeof(int), 1, f);
    fwrite(&outputCount, sizeof(int), 1, f);
  }

  // Collect unique connections (only from output side to avoid duplicates)
  std::vector<ConnectionDefinition> connections;
  for (auto *node : nodes) {
    for (const auto &conn : node->connections) {
      if (conn.outputNode == node) {
        ConnectionDefinition cd;
        cd.inputNodeId = nodePtrToId[(Node *)conn.inputNode];
        cd.inputSlot = conn.inputSlot;
        cd.outputNodeId = nodePtrToId[(Node *)conn.outputNode];
        cd.outputSlot = conn.outputSlot;
        connections.push_back(cd);
      }
    }
  }

  // Write connection count
  size_t connCount = connections.size();
  fwrite(&connCount, sizeof(size_t), 1, f);

  // Write connections
  for (const auto &conn : connections) {
    fwrite(&conn.inputNodeId, sizeof(int), 1, f);

    size_t inSlotLen = conn.inputSlot.size();
    fwrite(&inSlotLen, sizeof(size_t), 1, f);
    fwrite(conn.inputSlot.c_str(), 1, inSlotLen, f);

    fwrite(&conn.outputNodeId, sizeof(int), 1, f);

    size_t outSlotLen = conn.outputSlot.size();
    fwrite(&outSlotLen, sizeof(size_t), 1, f);
    fwrite(conn.outputSlot.c_str(), 1, outSlotLen, f);
  }

  fclose(f);
}

void NodeEditor::LoadScene(const std::string &filename) {
  FILE *f = fopen(filename.c_str(), "rb");
  if (!f)
    return;

  // Verify magic number
  char magic[4];
  fread(magic, 1, 4, f);

  bool isV1 = (magic[0] == 'B' && magic[1] == 'P' && magic[2] == 'S' &&
               magic[3] == '1');
  bool isV2 = (magic[0] == 'B' && magic[1] == 'P' && magic[2] == 'S' &&
               magic[3] == '2');

  if (!isV1 && !isV2) {
    fclose(f);
    return; // Invalid file format
  }

  // Clear existing nodes and state
  for (auto *node : nodes) {
    delete node;
  }
  nodes.clear();
  missingGateTypes.clear();
  placeholderNodes.clear();
  showMissingGatesBanner = false;

  // Read custom gate dependency section (BPS2 only)
  if (isV2) {
    size_t customTypeCount = 0;
    fread(&customTypeCount, sizeof(size_t), 1, f);

    for (size_t i = 0; i < customTypeCount; i++) {
      size_t len = 0;
      fread(&len, sizeof(size_t), 1, f);
      std::string typeName;
      typeName.resize(len);
      fread(&typeName[0], 1, len, f);

      // Check if this custom type is available
      if (!CustomGate::GateRegistry.count(typeName)) {
        // Type is missing - add to missing list if not already there
        if (std::find(missingGateTypes.begin(), missingGateTypes.end(),
                      typeName) == missingGateTypes.end()) {
          missingGateTypes.push_back(typeName);
        }
      }
    }

    if (!missingGateTypes.empty()) {
      showMissingGatesBanner = true;
      debugMsg =
          "Missing gates detected: " + std::to_string(missingGateTypes.size());
    }
  }

  // Read node count
  size_t nodeCount = 0;
  fread(&nodeCount, sizeof(size_t), 1, f);

  // Map for ID to node pointer
  std::map<int, Node *> idToNode;

  // Read nodes
  for (size_t i = 0; i < nodeCount; i++) {
    // Node type
    size_t typeLen = 0;
    fread(&typeLen, sizeof(size_t), 1, f);
    std::string type;
    type.resize(typeLen);
    fread(&type[0], 1, typeLen, f);

    // Node position
    ImVec2 pos;
    fread(&pos, sizeof(ImVec2), 1, f);

    // Slot counts (BPS2 only)
    int inputCount = 1;
    int outputCount = 1;
    if (isV2) {
      fread(&inputCount, sizeof(int), 1, f);
      fread(&outputCount, sizeof(int), 1, f);
    }

    // Create node (use placeholder for missing custom gates)
    Node *node = CreateNodeByType(type);
    if (!node && !IsBuiltInType(type)) {
      // Missing custom gate - create placeholder
      node = new PlaceholderGate(type, inputCount, outputCount);
      placeholderNodes.insert(static_cast<PlaceholderGate *>(node));
    }

    if (node) {
      node->pos = pos;
      node->id = "n" + std::to_string(i);
      nodes.push_back(node);
      idToNode[(int)i] = node;
    }
  }

  // Read connection count
  size_t connCount = 0;
  fread(&connCount, sizeof(size_t), 1, f);

  // Read connections
  for (size_t i = 0; i < connCount; i++) {
    int inputNodeId;
    fread(&inputNodeId, sizeof(int), 1, f);

    size_t inSlotLen = 0;
    fread(&inSlotLen, sizeof(size_t), 1, f);
    std::string inputSlot;
    inputSlot.resize(inSlotLen);
    fread(&inputSlot[0], 1, inSlotLen, f);

    int outputNodeId;
    fread(&outputNodeId, sizeof(int), 1, f);

    size_t outSlotLen = 0;
    fread(&outSlotLen, sizeof(size_t), 1, f);
    std::string outputSlot;
    outputSlot.resize(outSlotLen);
    fread(&outputSlot[0], 1, outSlotLen, f);

    // Create connection if both nodes exist
    if (idToNode.count(inputNodeId) && idToNode.count(outputNodeId)) {
      Connection conn;
      conn.inputNode = idToNode[inputNodeId];
      conn.inputSlot = inputSlot;
      conn.outputNode = idToNode[outputNodeId];
      conn.outputSlot = outputSlot;

      ((Node *)conn.inputNode)->connections.push_back(conn);
      ((Node *)conn.outputNode)->connections.push_back(conn);
    }
  }

  fclose(f);

  // Update script from loaded nodes
  UpdateScriptFromNodes();
  lastParsedScript = currentScript;
}

} // namespace Billyprints
