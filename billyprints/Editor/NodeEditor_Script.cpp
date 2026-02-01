#include "../Nodes/Gates/CustomGate.hpp"
#include "../Nodes/Special/PinIn.hpp"
#include "NodeEditor.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace Billyprints {

// Helper to trim whitespace
static void trimStr(std::string &s) {
  if (s.empty())
    return;
  s.erase(0, s.find_first_not_of(" \t\n\r"));
  size_t last = s.find_last_not_of(" \t\n\r");
  if (last != std::string::npos)
    s.erase(last + 1);
}

// Helper to split string by delimiter
static std::vector<std::string> splitStr(const std::string &s, char delim) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    trimStr(item);
    if (!item.empty())
      result.push_back(item);
  }
  return result;
}

// Parse and register a custom gate definition from script
// Syntax: define Name(in1, in2) -> (out1, out2):
//           out1 = in1 OP in2
//         end
static bool ParseGateDefinition(const std::string &defBlock,
                                std::string &errorOut) {
  std::stringstream ss(defBlock);
  std::string line;
  std::string gateName;
  std::vector<std::string> inputs;
  std::vector<std::string> outputs;
  std::vector<std::pair<std::string, std::string>> assignments; // output = expr

  // Parse first line: define Name(in1, in2) -> (out1, out2):
  if (!std::getline(ss, line)) {
    errorOut = "Empty define block";
    return false;
  }
  trimStr(line);

  // Remove "define " prefix
  if (line.substr(0, 7) != "define ") {
    errorOut = "Block must start with 'define'";
    return false;
  }
  line = line.substr(7);
  trimStr(line);

  // Extract gate name
  size_t parenPos = line.find('(');
  if (parenPos == std::string::npos) {
    errorOut = "Missing '(' in define";
    return false;
  }
  gateName = line.substr(0, parenPos);
  trimStr(gateName);

  // Extract inputs: between ( and )
  size_t closeParenPos = line.find(')');
  if (closeParenPos == std::string::npos || closeParenPos <= parenPos) {
    errorOut = "Missing ')' for inputs";
    return false;
  }
  std::string inputsStr = line.substr(parenPos + 1, closeParenPos - parenPos - 1);
  inputs = splitStr(inputsStr, ',');

  // Find -> and outputs
  size_t arrowPos = line.find("->");
  if (arrowPos == std::string::npos) {
    errorOut = "Missing '->' in define";
    return false;
  }

  std::string afterArrow = line.substr(arrowPos + 2);
  trimStr(afterArrow);

  // Extract outputs: between ( and ):
  size_t outOpenParen = afterArrow.find('(');
  size_t outCloseParen = afterArrow.find(')');
  if (outOpenParen == std::string::npos || outCloseParen == std::string::npos) {
    errorOut = "Missing output parentheses";
    return false;
  }
  std::string outputsStr =
      afterArrow.substr(outOpenParen + 1, outCloseParen - outOpenParen - 1);
  outputs = splitStr(outputsStr, ',');

  if (gateName.empty() || inputs.empty() || outputs.empty()) {
    errorOut = "Gate must have name, inputs, and outputs";
    return false;
  }

  // Parse body: assignments like "out = in1 OP in2" or "out = NOT in1"
  while (std::getline(ss, line)) {
    trimStr(line);
    if (line.empty() || line == "end")
      continue;
    if (line[0] == '/' && line.size() > 1 && line[1] == '/')
      continue;

    size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) {
      errorOut = "Invalid assignment: " + line;
      return false;
    }
    std::string lhs = line.substr(0, eqPos);
    std::string rhs = line.substr(eqPos + 1);
    trimStr(lhs);
    trimStr(rhs);
    assignments.push_back({lhs, rhs});
  }

  // Now build the GateDefinition
  GateDefinition def;
  def.name = gateName;
  def.color = IM_COL32(60, 80, 120, 200); // Default blue-ish color
  def.inputPinNames = inputs;   // Store original parameter names (a, b, etc.)
  def.outputPinNames = outputs; // Store original output names (out, etc.)

  // Signal tracking: maps signal name to (nodeId, outputSlotName)
  // Allows accessing multi-output gates via signal.outputName
  struct Signal {
    int nodeId;
    std::string slot; // output slot name
  };
  std::map<std::string, Signal> signals;
  int nodeIdCounter = 0;
  float yPos = 0;

  // Create PinIn nodes for each input
  for (const auto &inputName : inputs) {
    NodeDefinition nd;
    nd.type = "In";
    nd.id = nodeIdCounter;
    nd.pos = ImVec2(0, yPos);
    yPos += 60;
    def.nodes.push_back(nd);
    def.inputPinIndices.push_back(nodeIdCounter);
    signals[inputName] = {nodeIdCounter, "out"};
    nodeIdCounter++;
  }

  // Create constant nodes for literals 0 and 1
  // These are PinIn nodes that stay at a fixed value
  int constLowId = -1;  // Will be created on demand
  int constHighId = -1;

  // Helper lambdas for creating nodes and connections
  auto createNode = [&](const std::string &type, float x, float y) -> int {
    NodeDefinition nd;
    nd.type = type;
    nd.id = nodeIdCounter;
    nd.pos = ImVec2(x, y);
    def.nodes.push_back(nd);
    return nodeIdCounter++;
  };

  auto connect = [&](int fromNode, const std::string &fromSlot, int toNode,
                     const std::string &toSlot) {
    ConnectionDefinition cd;
    cd.outputNodeId = fromNode;
    cd.outputSlot = fromSlot;
    cd.inputNodeId = toNode;
    cd.inputSlot = toSlot;
    def.connections.push_back(cd);
  };

  // Process each assignment to create gate nodes
  float gateX = 150;
  float gateY = 0;

  // Recursive expression parser that handles nested expressions like NOT (a AND b)
  // Returns Signal (nodeId + output slot), or nodeId=-1 on error
  std::function<Signal(const std::string&, std::string&)> parseExpr;
  parseExpr = [&](const std::string& exprIn, std::string& err) -> Signal {
    std::string expr = exprIn;
    trimStr(expr);

    // Handle literal 0 - create constant low PinIn
    if (expr == "0") {
      if (constLowId < 0) {
        NodeDefinition nd;
        nd.type = "In";
        nd.id = nodeIdCounter;
        nd.pos = ImVec2(-100, 0);
        def.nodes.push_back(nd);
        constLowId = nodeIdCounter++;
      }
      return {constLowId, "out"};
    }

    // Handle literal 1 - create constant high PinIn (will need special handling)
    if (expr == "1") {
      if (constHighId < 0) {
        // Create a constant high: In -> NOT -> NOT (double invert stays high when In is low)
        // Actually simpler: just create an In that we'll mark as initially on
        // For now, use: In with value=true would require special node type
        // Workaround: NOT(0) = 1
        if (constLowId < 0) {
          NodeDefinition nd;
          nd.type = "In";
          nd.id = nodeIdCounter;
          nd.pos = ImVec2(-100, 0);
          def.nodes.push_back(nd);
          constLowId = nodeIdCounter++;
        }
        int notGate = createNode("NOT", -50, 0);
        connect(constLowId, "out", notGate, "in");
        constHighId = notGate;
      }
      return {constHighId, "out"};
    }

    // Check for dot notation: signal.outputName (accessing multi-output gate)
    size_t dotPos = expr.find('.');
    if (dotPos != std::string::npos) {
      std::string baseName = expr.substr(0, dotPos);
      std::string outputName = expr.substr(dotPos + 1);
      trimStr(baseName);
      trimStr(outputName);
      if (signals.count(baseName)) {
        // Return the same node but with the specified output slot
        Signal base = signals[baseName];
        return {base.nodeId, outputName};
      }
    }

    // Remove outer parentheses if present: (a AND b) -> a AND b
    while (expr.size() >= 2 && expr[0] == '(' && expr[expr.size()-1] == ')') {
      int depth = 0;
      bool matched = true;
      for (size_t i = 0; i < expr.size() - 1; ++i) {
        if (expr[i] == '(') depth++;
        else if (expr[i] == ')') depth--;
        if (depth == 0 && i > 0) { matched = false; break; }
      }
      if (matched) {
        expr = expr.substr(1, expr.size() - 2);
        trimStr(expr);
      } else break;
    }

    // Check for NOT (unary) - handles both "NOT a" and "NOT (a AND b)"
    if (expr.size() > 4 && expr.substr(0, 4) == "NOT ") {
      std::string inner = expr.substr(4);
      trimStr(inner);
      Signal innerSig = parseExpr(inner, err);
      if (innerSig.nodeId < 0) return {-1, ""};
      int notGate = createNode("NOT", gateX, gateY);
      gateY += 50;
      connect(innerSig.nodeId, innerSig.slot, notGate, "in");
      return {notGate, "out"};
    }

    // Check for AND (binary) - find AND not inside parentheses
    {
      int depth = 0;
      for (size_t i = 0; i + 5 <= expr.size(); ++i) {
        if (expr[i] == '(') depth++;
        else if (expr[i] == ')') depth--;
        else if (depth == 0 && expr.substr(i, 5) == " AND ") {
          std::string left = expr.substr(0, i);
          std::string right = expr.substr(i + 5);
          trimStr(left);
          trimStr(right);
          Signal leftSig = parseExpr(left, err);
          if (leftSig.nodeId < 0) return {-1, ""};
          Signal rightSig = parseExpr(right, err);
          if (rightSig.nodeId < 0) return {-1, ""};
          int andGate = createNode("AND", gateX, gateY);
          gateY += 50;
          connect(leftSig.nodeId, leftSig.slot, andGate, "in0");
          connect(rightSig.nodeId, rightSig.slot, andGate, "in1");
          return {andGate, "out"};
        }
      }
    }

    // Check for OR (binary) - find OR not inside parentheses
    {
      int depth = 0;
      for (size_t i = 0; i + 4 <= expr.size(); ++i) {
        if (expr[i] == '(') depth++;
        else if (expr[i] == ')') depth--;
        else if (depth == 0 && expr.substr(i, 4) == " OR ") {
          std::string left = expr.substr(0, i);
          std::string right = expr.substr(i + 4);
          trimStr(left);
          trimStr(right);
          Signal leftSig = parseExpr(left, err);
          if (leftSig.nodeId < 0) return {-1, ""};
          Signal rightSig = parseExpr(right, err);
          if (rightSig.nodeId < 0) return {-1, ""};
          // Check if OR is defined as custom gate
          if (CustomGate::GateRegistry.count("OR")) {
            int orGate = createNode("OR", gateX, gateY);
            gateY += 60;
            const auto &gateDef = CustomGate::GateRegistry["OR"];
            std::string in0Slot = (gateDef.inputPinIndices.size() == 1) ? "in" : "in0";
            std::string in1Slot = (gateDef.inputPinIndices.size() == 1) ? "in" : "in1";
            connect(leftSig.nodeId, leftSig.slot, orGate, in0Slot);
            connect(rightSig.nodeId, rightSig.slot, orGate, in1Slot);
            return {orGate, "out"};
          }
          // Build OR from NOT and AND: OR(a,b) = NOT(NOT a AND NOT b)
          int notLeft = createNode("NOT", gateX, gateY); gateY += 50;
          connect(leftSig.nodeId, leftSig.slot, notLeft, "in");
          int notRight = createNode("NOT", gateX, gateY); gateY += 50;
          connect(rightSig.nodeId, rightSig.slot, notRight, "in");
          int andGate = createNode("AND", gateX, gateY); gateY += 50;
          connect(notLeft, "out", andGate, "in0");
          connect(notRight, "out", andGate, "in1");
          int notResult = createNode("NOT", gateX, gateY); gateY += 50;
          connect(andGate, "out", notResult, "in");
          return {notResult, "out"};
        }
      }
    }

    // Check for custom gate call: GateName(arg1, arg2, ...)
    size_t parenPos = expr.find('(');
    if (parenPos != std::string::npos && parenPos > 0) {
      size_t closePos = expr.rfind(')');
      if (closePos != std::string::npos && closePos > parenPos) {
        std::string gateType = expr.substr(0, parenPos);
        trimStr(gateType);
        std::string argsStr = expr.substr(parenPos + 1, closePos - parenPos - 1);
        std::vector<std::string> callArgs = splitStr(argsStr, ',');

        if (!CustomGate::GateRegistry.count(gateType)) {
          err = "Unknown gate type: " + gateType;
          return {-1, ""};
        }

        // Recursively parse each argument
        std::vector<Signal> argSigs;
        for (const auto &arg : callArgs) {
          Signal argSig = parseExpr(arg, err);
          if (argSig.nodeId < 0) return {-1, ""};
          argSigs.push_back(argSig);
        }

        int customGate = createNode(gateType, gateX, gateY);
        gateY += 60;

        const auto &gateDef = CustomGate::GateRegistry[gateType];
        for (size_t i = 0; i < argSigs.size() && i < gateDef.inputPinIndices.size(); ++i) {
          std::string inSlot = (gateDef.inputPinIndices.size() == 1) ? "in" : "in" + std::to_string(i);
          connect(argSigs[i].nodeId, argSigs[i].slot, customGate, inSlot);
        }
        // Default to first output slot
        std::string outSlot = (gateDef.outputPinIndices.size() == 1) ? "out" : "out0";
        return {customGate, outSlot};
      }
    }

    // Must be a signal reference
    if (signals.count(expr)) {
      return signals[expr];
    }

    err = "Unknown signal: " + expr;
    return {-1, ""};
  };

  for (const auto &[outSignal, expr] : assignments) {
    std::string parseErr;
    Signal resultSig = parseExpr(expr, parseErr);
    if (resultSig.nodeId < 0) {
      errorOut = parseErr;
      return false;
    }
    signals[outSignal] = resultSig;
  }

  // Create PinOut nodes for each output
  float outX = 300;
  float outY = 0;
  for (const auto &outputName : outputs) {
    NodeDefinition nd;
    nd.type = "Out";
    nd.id = nodeIdCounter;
    nd.pos = ImVec2(outX, outY);
    outY += 60;
    def.nodes.push_back(nd);
    def.outputPinIndices.push_back(nodeIdCounter);

    // Connect the signal to this output
    if (signals.count(outputName)) {
      ConnectionDefinition cd;
      cd.outputNodeId = signals[outputName].nodeId;
      cd.outputSlot = signals[outputName].slot;
      cd.inputNodeId = nodeIdCounter;
      cd.inputSlot = "in";
      def.connections.push_back(cd);
    } else {
      errorOut = "Output signal not defined: " + outputName;
      return false;
    }
    nodeIdCounter++;
  }

  // Register the gate
  CustomGate::GateRegistry[def.name] = def;

  return true;
}

// Extract all define...end blocks from script and parse them
// Returns the define blocks in 'definitions' for preservation
static std::string ExtractAndParseDefinitions(const std::string &script,
                                              std::string &remaining,
                                              std::string &definitions,
                                              std::string &errorOut) {
  remaining = "";
  definitions = "";
  std::stringstream ss(script);
  std::string line;
  bool inDefine = false;
  std::string currentDefine;
  std::string outsideDefine;
  std::string allDefinitions;

  while (std::getline(ss, line)) {
    std::string trimmed = line;
    trimStr(trimmed);

    if (!inDefine && trimmed.substr(0, 7) == "define ") {
      inDefine = true;
      currentDefine = line + "\n";
    } else if (inDefine) {
      currentDefine += line + "\n";
      if (trimmed == "end") {
        // Parse this definition
        std::string err;
        if (!ParseGateDefinition(currentDefine, err)) {
          errorOut += "Define error: " + err + "\n";
        } else {
          // Successfully parsed, preserve the block
          allDefinitions += currentDefine + "\n";
        }
        inDefine = false;
        currentDefine = "";
      }
    } else {
      outsideDefine += line + "\n";
    }
  }

  if (inDefine) {
    errorOut += "Unclosed define block\n";
  }

  remaining = outsideDefine;
  definitions = allDefinitions;
  return errorOut;
}

void NodeEditor::UpdateScriptFromNodes() {
  std::stringstream ss;
  std::map<Node *, std::string> nodeToId;
  int autoIdCounter = 0;

  // Include any preserved gate definitions at the top
  if (!scriptDefinitions.empty()) {
    ss << scriptDefinitions;
  }

  // First pass: Assign IDs
  for (int i = 0; i < nodes.size(); ++i) {
    if (nodes[i]->id.empty()) {
      // Find a unique ID
      while (true) {
        std::string candidate = "n" + std::to_string(autoIdCounter++);
        bool clash = false;
        for (const auto &n : nodes) {
          if (n->id == candidate) {
            clash = true;
            break;
          }
        }
        if (!clash) {
          nodes[i]->id = candidate;
          break;
        }
      }
    }
    nodeToId[nodes[i]] = nodes[i]->id;
  }

  for (int i = 0; i < nodes.size(); ++i) {
    std::string type = nodes[i]->title;
    ss << type << " " << nodes[i]->id << " @ " << (int)nodes[i]->pos.x << ", "
       << (int)nodes[i]->pos.y;

    if (type == "In") {
      PinIn *pin = (PinIn *)nodes[i];
      if (pin->isMomentary)
        ss << " momentary";
    }

    ss << "\n";
  }
  ss << "\n";
  for (auto *node : nodes) {
    for (const auto &conn : node->connections) {
      if (conn.outputNode == node) {
        ss << nodeToId[(Node *)conn.outputNode] << "." << conn.outputSlot
           << " -> " << nodeToId[(Node *)conn.inputNode] << "."
           << conn.inputSlot << "\n";
      }
    }
  }
  currentScript = ss.str();
}

// Helper to resolve a slot name for a custom gate
// Accepts both named pins (a, b) and indexed pins (in0, in1)
// Returns the actual slot name used by the node (in0, in1, etc.)
static std::string ResolveSlotName(Node* node, const std::string& slotName, bool isInput) {
  // Check if this is a custom gate
  if (CustomGate::GateRegistry.count(node->title)) {
    const auto& def = CustomGate::GateRegistry[node->title];

    if (isInput) {
      // Check if slotName matches a defined input name (a, b, etc.)
      for (size_t i = 0; i < def.inputPinNames.size(); ++i) {
        if (def.inputPinNames[i] == slotName) {
          // Map to indexed slot name
          if (def.inputPinNames.size() == 1)
            return "in";
          else
            return "in" + std::to_string(i);
        }
      }
    } else {
      // Check if slotName matches a defined output name
      for (size_t i = 0; i < def.outputPinNames.size(); ++i) {
        if (def.outputPinNames[i] == slotName) {
          // Map to indexed slot name
          if (def.outputPinNames.size() == 1)
            return "out";
          else
            return "out" + std::to_string(i);
        }
      }
    }
  }

  // Not a custom gate or slot name didn't match defined names
  // Return as-is (might be in0, in1, out, etc.)
  return slotName;
}

void NodeEditor::UpdateNodesFromScript() {
  if (currentScript == lastParsedScript)
    return;
  lastParsedScript = currentScript;
  scriptError = "";

  auto trim = [](std::string &s) {
    if (s.empty())
      return;
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    size_t last = s.find_last_not_of(" \t\n\r");
    if (last != std::string::npos)
      s.erase(last + 1);
  };

  for (auto *n : nodes)
    delete n;
  nodes.clear();

  // First pass: Extract and parse custom gate definitions
  std::string remainingScript;
  std::string defErrors;
  ExtractAndParseDefinitions(currentScript, remainingScript, scriptDefinitions,
                             defErrors);
  if (!defErrors.empty()) {
    scriptError += defErrors;
  }

  // Second pass: Parse nodes and connections from remaining script
  std::stringstream ss(remainingScript);
  std::string line;
  std::map<std::string, Node *> idToNode;
  int lineNum = 0;

  while (std::getline(ss, line)) {
    lineNum++;
    trim(line);
    if (line.empty() || (line.size() >= 2 && line[0] == '/' && line[1] == '/'))
      continue;

    try {
      if (line.find("->") != std::string::npos) {
        size_t arrowPos = line.find("->");
        std::string left = line.substr(0, arrowPos);
        std::string right = line.substr(arrowPos + 2);
        trim(left);
        trim(right);

        auto parseSlot =
            [&](std::string s,
                bool isOutput) -> std::pair<std::string, std::string> {
          size_t dot = s.find('.');
          if (dot == std::string::npos) {
            return {s, isOutput ? "out" : "in"};
          }
          std::string nodePart = s.substr(0, dot);
          std::string slotPart = s.substr(dot + 1);
          trim(nodePart);
          trim(slotPart);
          return {nodePart, slotPart};
        };

        auto outS = parseSlot(left, true);
        auto inS = parseSlot(right, false);

        if (outS.first.empty() || inS.first.empty() || outS.second.empty() ||
            inS.second.empty())
          continue;

        if (idToNode.count(outS.first) && idToNode.count(inS.first)) {
          Node *outNode = idToNode[outS.first];
          Node *inNode = idToNode[inS.first];

          // Resolve slot names (translates named pins like "a" to indexed "in0")
          std::string resolvedOutSlot = ResolveSlotName(outNode, outS.second, false);
          std::string resolvedInSlot = ResolveSlotName(inNode, inS.second, true);

          bool outSlotValid = false;
          if (resolvedOutSlot == "out" && std::string(outNode->title) == "In")
            outSlotValid = true;
          else {
            for (int i = 0; i < outNode->outputSlotCount; ++i)
              if (std::string(outNode->outputSlots[i].title) == resolvedOutSlot)
                outSlotValid = true;
          }

          bool inSlotValid = false;
          if (resolvedInSlot == "in" && std::string(inNode->title) == "Out")
            inSlotValid = true;
          else {
            for (int i = 0; i < inNode->inputSlotCount; ++i)
              if (std::string(inNode->inputSlots[i].title) == resolvedInSlot)
                inSlotValid = true;
          }

          if (outSlotValid && inSlotValid) {
            Connection conn;
            conn.outputNode = outNode;
            conn.outputSlot = resolvedOutSlot;
            conn.inputNode = inNode;
            conn.inputSlot = resolvedInSlot;
            outNode->connections.push_back(conn);
            inNode->connections.push_back(conn);
          }
        }
      } else if (line.find("@") != std::string::npos) {
        std::stringstream lss(line);
        std::string type, id, at;
        int x, y;
        char comma;
        if (!(lss >> type >> id >> at >> x >> comma >> y)) {
          scriptError +=
              "Line " + std::to_string(lineNum) + ": Invalid node format\n";
          continue;
        }

        Node *n = CreateNodeByType(type);
        if (n) {
          n->pos = {(float)x, (float)y};
          n->id = id;
          if (type == "In" && line.find("momentary") != std::string::npos) {
            ((PinIn *)n)->isMomentary = true;
          }
          nodes.push_back(n);
          idToNode[id] = n;
        } else {
          scriptError += "Line " + std::to_string(lineNum) + ": Unknown type " +
                         type + "\n";
        }
      }
    } catch (...) {
      scriptError += "Line " + std::to_string(lineNum) + ": Unexpected error\n";
    }
  }
}
} // namespace Billyprints
