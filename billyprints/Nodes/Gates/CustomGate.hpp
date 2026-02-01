#pragma once

#include "../Special/PinIn.hpp"
#include "../Special/PinOut.hpp"
#include "Gate.hpp"
#include <map>
#include <string>
#include <vector>

namespace Billyprints {

struct NodeDefinition {
  std::string type;
  ImVec2 pos;
  int id; // Original ID in the definition
};

struct ConnectionDefinition {
  int inputNodeId;
  std::string inputSlot;
  int outputNodeId;
  std::string outputSlot;
};

struct GateDefinition {
  std::string name;
  std::vector<NodeDefinition> nodes;
  std::vector<ConnectionDefinition> connections;
  std::vector<int> inputPinIndices;  // IDs of PinIn nodes in 'nodes' vector
  std::vector<int> outputPinIndices; // IDs of PinOut nodes in 'nodes' vector
  std::vector<std::string> inputPinNames;  // Original parameter names (a, b, etc.)
  std::vector<std::string> outputPinNames; // Original output names (out, result, etc.)
  ImU32 color = IM_COL32(50, 50, 50, 200); // Default dark grey
};

Node *CreateNodeByType(const std::string &type);
Node *CreateNodeByTypeOrPlaceholder(const std::string &type, int inputHint = 1,
                                    int outputHint = 1);

class CustomGate : public Gate {
public:
  CustomGate(const GateDefinition &def);
  ~CustomGate();

  bool Evaluate() override;
  ImU32 GetColor() const override { return definition.color; }

  // Members to hold the internal state
  std::vector<Node *> internalNodes;
  std::vector<Connection> internalConnections;

  // Pointers to the interface pins within internalNodes
  std::vector<PinIn *> internalInputs;
  std::vector<PinOut *> internalOutputs;

  // Registry for all custom gates
  static std::map<std::string, GateDefinition> GateRegistry;

private:
  GateDefinition definition;
};
} // namespace Billyprints
