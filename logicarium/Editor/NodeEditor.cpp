#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "NodeEditor.hpp"
#include "AI/AIAssistant.hpp"
#include <imgui_internal.h>
#include <map>
#include <string>
#include <vector>

namespace Logicarium {
inline void NodeEditor::RenderNode(Node *node) { node->Render(); }

Node *nodeToDuplicate = nullptr;
Node *nodeToEdit = nullptr;
Node *nodeToDelete = nullptr;
Node *nodeToSaveGate = nullptr;
Node *nodeToRename = nullptr;
char renameBuf[128] = "";
bool nodeHoveredForContextMenu = false;

void NodeEditor::RenderDock() {
  if (!showDock)
    return;

  float dockHeight = 84.0f;
  float iconSize = 48.0f;
  float iconPadding = 20.0f;

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImVec2 workPos = viewport->WorkPos;
  ImVec2 workSize = viewport->WorkSize;

  size_t totalIcons = availableNodes.size() + availableGates.size();
  float totalWidth = totalIcons * (iconSize + iconPadding) + iconPadding;

  ImVec2 dockPos = ImVec2(workPos.x + (workSize.x - totalWidth) * 0.5f,
                          workPos.y + workSize.y - dockHeight - 30.0f);
  ImVec2 dockSize = ImVec2(totalWidth, dockHeight);

  // Use Foreground draw list to overlay everything
  ImDrawList *drawList = ImGui::GetForegroundDrawList();

  float dockAlphaMultiplier = anyNodeDragged ? 0.35f : 1.0f;

  // Background Shadow/Glow
  drawList->AddRectFilled(
      ImVec2(dockPos.x - 5, dockPos.y - 5),
      ImVec2(dockPos.x + dockSize.x + 5, dockPos.y + dockSize.y + 5),
      IM_COL32(0, 0, 0, (int)(60 * dockAlphaMultiplier)), 16.0f);

  // Glassmorphic Body
  drawList->AddRectFilled(
      dockPos, ImVec2(dockPos.x + dockSize.x, dockPos.y + dockSize.y),
      IM_COL32(30, 35, 45, (int)(180 * dockAlphaMultiplier)), 14.0f);
  drawList->AddRect(
      dockPos, ImVec2(dockPos.x + dockSize.x, dockPos.y + dockSize.y),
      IM_COL32(255, 255, 255, (int)(40 * dockAlphaMultiplier)), 14.0f, 0, 2.0f);

  float xOffset = iconPadding;
  ImVec2 mousePos = ImGui::GetMousePos();

  auto renderIcon = [&](const char *label, std::function<Node *()> factory,
                        ImU32 color) {
    ImVec2 center = ImVec2(dockPos.x + xOffset + iconSize * 0.5f,
                           dockPos.y + dockHeight * 0.5f);
    bool hovered = (mousePos.x >= dockPos.x + xOffset &&
                    mousePos.x <= dockPos.x + xOffset + iconSize &&
                    mousePos.y >= dockPos.y + (dockHeight - iconSize) * 0.5f &&
                    mousePos.y <= dockPos.y + (dockHeight + iconSize) * 0.5f);

    float scale = 1.0f;
    float lift = 0.0f;
    float currentSize = iconSize;

    ImVec2 animatedCenter = center;

    // Icon Circle
    drawList->AddCircleFilled(
        animatedCenter, currentSize * 0.5f,
        (color & 0x00FFFFFF) | ((int)(0xDD * dockAlphaMultiplier) << 24), 32);
    drawList->AddCircle(
        animatedCenter, currentSize * 0.5f,
        IM_COL32(255, 255, 255, (int)(80 * dockAlphaMultiplier)), 32, 1.5f);

    // Symbol/Label in center
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font
    std::string shortLabel = label;
    if (shortLabel.size() > 4)
      shortLabel = shortLabel.substr(0, 3) + ".";

    float fontSize = 14.0f * scale;
    ImVec2 textSize = ImGui::CalcTextSize(shortLabel.c_str());
    drawList->AddText(
        NULL, fontSize,
        ImVec2(animatedCenter.x -
                   textSize.x * 0.5f * (fontSize / ImGui::GetFontSize()),
               animatedCenter.y -
                   textSize.y * 0.5f * (fontSize / ImGui::GetFontSize())),
        IM_COL32(255, 255, 255, (int)(255 * dockAlphaMultiplier)),
        shortLabel.c_str());
    ImGui::PopFont();

    // Full Label Tooltip or hint
    if (hovered) {
      ImGui::BeginTooltip();
      ImGui::Text("%s", label);
      ImGui::EndTooltip();

      if (ImGui::IsMouseClicked(0)) {
        Node *newNode = factory();
        nodes.push_back(newNode);
        ImNodes::AutoPositionNode(newNode);
        // Attempt to make the node active immediately for dragging
        ImGui::SetActiveID(ImGui::GetID(newNode), ImGui::GetCurrentWindow());
      }
    }

    xOffset += iconSize + iconPadding;
  };

  for (auto &f : availableNodes) {
    Node *tmp = f();
    renderIcon(tmp->title, f, tmp->GetColor());
    delete tmp;
  }

  for (auto &f : availableGates) {
    Node *tmp = f();
    renderIcon(tmp->title, f, tmp->GetColor());
    delete tmp;
  }
}

void NodeEditor::DuplicateNode(Node *node) {
  if (!node)
    return;
  Node *newNode = CreateNodeByType(node->title);
  if (newNode) {
    newNode->pos = ImVec2(node->pos.x + 30.0f, node->pos.y + 30.0f);
    nodes.push_back(newNode);
    ImNodes::AutoPositionNode(newNode);
  }
}

void NodeEditor::SelectAllNodes() {
  for (auto *node : nodes) {
    node->selected = true;
  }
}

void NodeEditor::DeselectAllNodes() {
  for (auto *node : nodes) {
    node->selected = false;
  }
}

void NodeEditor::DuplicateSelectedNodes() {
  std::vector<Node *> selected;
  for (auto *node : nodes) {
    if (node->selected) {
      selected.push_back(node);
    }
  }

  // Map from original node to duplicated node
  std::map<Node *, Node *> originalToDuplicate;

  // Deselect originals, duplicate and select new ones
  for (auto *node : selected) {
    node->selected = false;
    Node *newNode = CreateNodeByType(node->title);
    if (newNode) {
      newNode->pos = ImVec2(node->pos.x + 30.0f, node->pos.y + 30.0f);
      newNode->selected = true;
      nodes.push_back(newNode);
      originalToDuplicate[node] = newNode;
    }
  }

  // Duplicate connections between selected nodes
  for (auto *node : selected) {
    for (const auto &conn : node->connections) {
      // Only process connections where this node is the output (to avoid
      // duplicating twice)
      if (conn.outputNode != node)
        continue;

      Node *inputNode = (Node *)conn.inputNode;
      Node *outputNode = (Node *)conn.outputNode;

      // Check if both ends of the connection are in the selected set
      if (originalToDuplicate.count(inputNode) &&
          originalToDuplicate.count(outputNode)) {
        Connection newConn;
        newConn.inputNode = originalToDuplicate[inputNode];
        newConn.inputSlot = conn.inputSlot;
        newConn.outputNode = originalToDuplicate[outputNode];
        newConn.outputSlot = conn.outputSlot;

        ((Node *)newConn.inputNode)->connections.push_back(newConn);
        ((Node *)newConn.outputNode)->connections.push_back(newConn);
      }
    }
  }
}

void NodeEditor::FrameSelectedNodes() {
  auto *canvas = ImNodes::GetCurrentCanvas();
  if (!canvas)
    return;

  // Find bounding box of selected nodes
  bool hasSelection = false;
  ImVec2 minPos(FLT_MAX, FLT_MAX);
  ImVec2 maxPos(-FLT_MAX, -FLT_MAX);

  for (auto *node : nodes) {
    if (node->selected) {
      hasSelection = true;
      minPos.x = ImMin(minPos.x, node->pos.x);
      minPos.y = ImMin(minPos.y, node->pos.y);
      maxPos.x =
          ImMax(maxPos.x, node->pos.x + 150.0f); // Approximate node width
      maxPos.y =
          ImMax(maxPos.y, node->pos.y + 80.0f); // Approximate node height
    }
  }

  if (!hasSelection) {
    // If nothing selected, frame all nodes
    for (auto *node : nodes) {
      minPos.x = ImMin(minPos.x, node->pos.x);
      minPos.y = ImMin(minPos.y, node->pos.y);
      maxPos.x = ImMax(maxPos.x, node->pos.x + 150.0f);
      maxPos.y = ImMax(maxPos.y, node->pos.y + 80.0f);
    }
  }

  if (minPos.x == FLT_MAX)
    return; // No nodes at all

  // Center the view on the bounding box
  ImVec2 center =
      ImVec2((minPos.x + maxPos.x) * 0.5f, (minPos.y + maxPos.y) * 0.5f);
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvas->Offset = ImVec2(canvasSize.x * 0.5f - center.x * canvas->Zoom,
                          canvasSize.y * 0.5f - center.y * canvas->Zoom);
}

void NodeEditor::ResetView() {
  auto *canvas = ImNodes::GetCurrentCanvas();
  if (!canvas)
    return;
  canvas->Zoom = 1.0f;
  canvas->Offset = ImVec2(0, 0);
}

void NodeEditor::HandleKeyBindings() {
  // Only handle keys when canvas window is focused and no text input is active
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    return;
  if (ImGui::GetIO().WantTextInput)
    return;

  bool ctrl = ImGui::GetIO().KeyCtrl;
  bool shift = ImGui::GetIO().KeyShift;
  auto *canvas = ImNodes::GetCurrentCanvas();

  // === SELECTION ===

  // Ctrl+A: Select all
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
    SelectAllNodes();
  }

  // Escape: Deselect all / cancel connection drop menu
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    if (showConnectionDropMenu) {
      showConnectionDropMenu = false;
    } else {
      DeselectAllNodes();
    }
  }

  // === EDITING ===

  // Ctrl+D: Duplicate selected
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
    DuplicateSelectedNodes();
  }

  // Delete or Backspace: Delete selected (Delete handled in RenderNodes)
  if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
    for (auto *node : nodes) {
      if (node->selected) {
        nodeToDelete = node;
        break;
      }
    }
  }

  // E: Edit selected node (opens gate editor)
  if (ImGui::IsKeyPressed(ImGuiKey_E) && !ctrl) {
    for (auto *node : nodes) {
      if (node->selected) {
        nodeToEdit = node;
        break;
      }
    }
  }

  // G: Create new gate
  if (ImGui::IsKeyPressed(ImGuiKey_G) && !ctrl) {
    openCreateGatePopup = true;
  }

  // === NAVIGATION ===

  // F: Frame selection (or all nodes if nothing selected)
  if (ImGui::IsKeyPressed(ImGuiKey_F) && !ctrl) {
    FrameSelectedNodes();
  }

  // Home: Reset view
  if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
    ResetView();
  }

  // +/=: Zoom in
  if (ImGui::IsKeyPressed(ImGuiKey_Equal) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
    if (canvas) {
      canvas->Zoom = ImMin(canvas->Zoom * 1.2f, 4.0f);
    }
  }

  // -: Zoom out
  if (ImGui::IsKeyPressed(ImGuiKey_Minus) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
    if (canvas) {
      canvas->Zoom = ImMax(canvas->Zoom / 1.2f, 0.25f);
    }
  }

  // Ctrl+0: Reset zoom only (keep position)
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_0)) {
    if (canvas) {
      canvas->Zoom = 1.0f;
    }
  }

  // === PANELS ===

  // Tab: Toggle script editor
  if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !ctrl) {
    showScriptEditor = !showScriptEditor;
  }

  // D: Toggle dock
  if (ImGui::IsKeyPressed(ImGuiKey_D) && !ctrl) {
    showDock = !showDock;
  }

  // === FILE OPERATIONS ===

  // Ctrl+S: Save scene
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    openSaveScenePopup = true;
  }

  // Ctrl+O: Open/Load scene
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
    openLoadScenePopup = true;
  }

  // === QUICK ACTIONS ===

  // Space: Open context menu at cursor (when not over a node)
  if (ImGui::IsKeyPressed(ImGuiKey_Space) && !nodeHoveredForContextMenu) {
    ImGui::OpenPopup("NodesContextMenu");
  }
}

void NodeEditor::UpdateGateDefinitionFromCurrentScene(const std::string &name) {
  for (auto &def : customGateDefinitions) {
    if (def.name == name) {
      def.nodes.clear();
      def.connections.clear();
      def.inputPinIndices.clear();
      def.outputPinIndices.clear();

      std::map<Node *, int> nodeToId;
      int idCounter = 0;

      for (auto *n : nodes) {
        NodeDefinition nDef;
        nDef.type = n->title;
        nDef.pos = n->pos;
        nDef.id = idCounter++;
        def.nodes.push_back(nDef);
        nodeToId[n] = nDef.id;

        if (nDef.type == "In")
          def.inputPinIndices.push_back(nDef.id);
        else if (nDef.type == "Out")
          def.outputPinIndices.push_back(nDef.id);
      }

      for (auto *n : nodes) {
        for (const auto &conn : n->connections) {
          if (conn.outputNode == n) {
            ConnectionDefinition cDef;
            cDef.outputNodeId = nodeToId[n];
            cDef.outputSlot = conn.outputSlot;
            cDef.inputNodeId = nodeToId[(Node *)conn.inputNode];
            cDef.inputSlot = conn.inputSlot;
            def.connections.push_back(cDef);
          }
        }
      }
      // Update the global registry as well
      CustomGate::GateRegistry[name] = def;
      break;
    }
  }
}

inline void NodeEditor::RenderNodes() {
  anyNodeDragged = false;
  for (auto it = nodes.begin(); it != nodes.end();) {
    Node *node = *it;

    RenderNode(node);

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
      anyNodeDragged = true;

    if (node->selected && ImGui::IsKeyPressedMap(ImGuiKey_Delete) &&
        ImGui::IsWindowFocused()) {
      for (auto &connection : node->connections) {
        if (connection.outputNode == node) {
          ((Node *)connection.inputNode)->DeleteConnection(connection);
        } else {
          ((Node *)connection.outputNode)->DeleteConnection(connection);
        }
      }
      node->connections.clear();

      delete node;
      it = nodes.erase(it);
    } else
      ++it;
  }
}

inline void NodeEditor::RenderContextMenu() {
  if (!nodeHoveredForContextMenu &&
      ImGui::BeginPopupContextWindow("NodesContextMenu",
                                     ImGuiPopupFlags_MouseButtonRight |
                                         ImGuiPopupFlags_NoOpenOverItems)) {
    for (const auto &desc : availableNodes) {
      auto item = desc();
      if (ImGui::MenuItem(item->title)) {
        nodes.push_back(item);
        ImNodes::AutoPositionNode(nodes.back());
      }
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Gates")) {
      for (const auto &desc : availableGates) {
        auto item = desc();
        if (ImGui::MenuItem(item->title)) {
          nodes.push_back(item);
          ImNodes::AutoPositionNode(nodes.back());
        } else {
          delete item; // Don't leak if not clicked
        }
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Reset Zoom"))
      ImNodes::GetCurrentCanvas()->Zoom = 1;
    ImGui::EndPopup();
  }
}

void NodeEditor::Redraw() {
  // Update AI assistant streaming
  if (aiAssistant) {
    aiAssistant->Update();
  }

  nodeHoveredForContextMenu = false;

  // Handle global interaction requests
  if (nodeToDuplicate) {
    DuplicateNode(nodeToDuplicate);
    nodeToDuplicate = nullptr;
  }
  if (nodeToEdit) {
    // Check if it's a custom gate or a standard gate
    bool isCustom = CustomGate::GateRegistry.count(nodeToEdit->title);

    if (isCustom) {
      originalSceneScript = currentScript;
      editingGateName = nodeToEdit->title;

      // Find the definition
      for (const auto &def : customGateDefinitions) {
        if (def.name == editingGateName) {
          // Clear current nodes
          for (auto *n : nodes)
            delete n;
          nodes.clear();

          // Map for ID reconstruction
          std::map<int, Node *> idToNode;

          // 1. Create nodes
          for (const auto &nodeDef : def.nodes) {
            Node *n = CreateNodeByType(nodeDef.type);
            if (n) {
              n->pos = nodeDef.pos;
              n->id = "n" + std::to_string(nodeDef.id);
              nodes.push_back(n);
              idToNode[nodeDef.id] = n;
            }
          }

          // 2. Create connections
          for (const auto &connDef : def.connections) {
            if (idToNode.count(connDef.inputNodeId) &&
                idToNode.count(connDef.outputNodeId)) {
              Connection conn;
              conn.inputNode = idToNode[connDef.inputNodeId];
              conn.inputSlot = connDef.inputSlot;
              conn.outputNode = idToNode[connDef.outputNodeId];
              conn.outputSlot = connDef.outputSlot;
              ((Node *)conn.inputNode)->connections.push_back(conn);
              ((Node *)conn.outputNode)->connections.push_back(conn);
            }
          }
          UpdateScriptFromNodes();
          lastParsedScript = ""; // Force reparse to ensure visual consistency
          UpdateNodesFromScript();
          break;
        }
      }
    } else {
      // Standard gate - open code editor
      gateBeingEdited = (Gate *)nodeToEdit;
      editingCode = gateBeingEdited->GetCode();
      showCodeEditor = true;
    }
    nodeToEdit = nullptr;
  }
  if (nodeToDelete) {
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
      if (*it == nodeToDelete) {
        // Handle connections
        for (auto &connection : (*it)->connections) {
          if (connection.outputNode == *it) {
            ((Node *)connection.inputNode)->DeleteConnection(connection);
          } else {
            ((Node *)connection.outputNode)->DeleteConnection(connection);
          }
        }
        delete *it;
        nodes.erase(it);
        break;
      }
    }
    nodeToDelete = nullptr;
  }

  // Handle saving a temporary gate to permanent library
  if (nodeToSaveGate) {
    std::string gateNameToSave = nodeToSaveGate->title;
    if (CustomGate::GateRegistry.count(gateNameToSave)) {
      auto &def = CustomGate::GateRegistry[gateNameToSave];
      if (def.isTemporary) {
        // Mark as permanent
        def.isTemporary = false;

        // Add to customGateDefinitions for serialization
        customGateDefinitions.push_back(def);

        // Add to availableGates to show in the dock
        availableGates.push_back(
            [def]() -> Gate * { return new CustomGate(def); });
      }
    }
    nodeToSaveGate = nullptr;
  }

  Node::GlobalFrameCount++;
  auto context = ImNodes::Ez::CreateContext();
  IM_UNUSED(context);

  // --- REMOVED REDUNDANT MENU BAR BLOCK ---

  // File Picker Helper
  auto RenderFilePicker = [&]() {
    ImGui::TextWrapped("Path: %s", currentPath.string().c_str());
    if (ImGui::Button("Up")) {
      if (currentPath.has_parent_path())
        currentPath = currentPath.parent_path();
    }

    ImGui::Separator();
    ImGui::BeginChild("FileList", ImVec2(400, 200), true);

    std::vector<std::filesystem::directory_entry> dirs, files;
    try {
      for (const auto &entry :
           std::filesystem::directory_iterator(currentPath)) {
        if (entry.is_directory())
          dirs.push_back(entry);
        else if (entry.path().extension() == ".bin")
          files.push_back(entry);
      }
    } catch (...) {
    }

    for (const auto &dir : dirs) {
      std::string dirName = "[DIR] " + dir.path().filename().string();
      if (ImGui::Selectable(dirName.c_str())) {
        currentPath = dir.path();
      }
    }

    for (const auto &file : files) {
      if (ImGui::Selectable(file.path().filename().string().c_str())) {
        strncpy(currentFilename, file.path().filename().string().c_str(), 128);
      }
    }

    ImGui::EndChild();
  };

  // Save Gate Popup
  if (openSaveGatePopup) {
    ImGui::OpenPopup("SaveGatePopup");
    openSaveGatePopup = false;
  }
  if (ImGui::BeginPopupModal("SaveGatePopup", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename", currentFilename, 128);

    RenderFilePicker();
    ImGui::Separator();

    if (ImGui::Button("Save", ImVec2(120, 0))) {
      std::filesystem::path fullPath = currentPath / currentFilename;
      SaveGates(fullPath.string());
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Load Gate Popup
  if (openLoadGatePopup) {
    ImGui::OpenPopup("LoadGatePopup");
    openLoadGatePopup = false;
  }
  if (ImGui::BeginPopupModal("LoadGatePopup", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename", currentFilename, 128);

    RenderFilePicker();
    ImGui::Separator();

    if (ImGui::Button("Load", ImVec2(120, 0))) {
      std::filesystem::path fullPath = currentPath / currentFilename;
      LoadGates(fullPath.string());
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Scene File Picker Helper (filters for .bps files)
  auto RenderSceneFilePicker = [&]() {
    ImGui::TextWrapped("Path: %s", currentPath.string().c_str());
    if (ImGui::Button("Up##Scene")) {
      if (currentPath.has_parent_path())
        currentPath = currentPath.parent_path();
    }

    ImGui::Separator();
    ImGui::BeginChild("SceneFileList", ImVec2(400, 200), true);

    std::vector<std::filesystem::directory_entry> dirs, files;
    try {
      for (const auto &entry :
           std::filesystem::directory_iterator(currentPath)) {
        if (entry.is_directory())
          dirs.push_back(entry);
        else if (entry.path().extension() == ".bps")
          files.push_back(entry);
      }
    } catch (...) {
    }

    for (const auto &dir : dirs) {
      std::string dirName = "[DIR] " + dir.path().filename().string();
      if (ImGui::Selectable(dirName.c_str())) {
        currentPath = dir.path();
      }
    }

    for (const auto &file : files) {
      if (ImGui::Selectable(file.path().filename().string().c_str())) {
        strncpy(sceneFilename, file.path().filename().string().c_str(), 128);
      }
    }

    ImGui::EndChild();
  };

  // Rename Node Popup
  static bool renameStarted = false;
  static bool renameFocusOnce = false;
  if (nodeToRename && !renameStarted) {
    ImGui::OpenPopup("RenameNodePopup");
    strncpy(renameBuf, nodeToRename->id.c_str(), 127);
    renameBuf[127] = '\0';
    renameStarted = true;
    renameFocusOnce = true;
  }

  if (ImGui::BeginPopupModal("RenameNodePopup", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Enter new name for pin:");

    if (renameFocusOnce) {
      ImGui::SetKeyboardFocusHere();
      renameFocusOnce = false;
    }

    ImGui::InputText("##rename", renameBuf, 128);

    if (ImGui::Button("Rename", ImVec2(120, 0)) ||
        ImGui::IsKeyPressed(ImGuiKey_Enter)) {
      std::string newName = renameBuf;
      if (!newName.empty() && newName.find(' ') == std::string::npos) {
        nodeToRename->id = newName;
        UpdateScriptFromNodes();
        lastParsedScript = currentScript;
        strncpy(scriptBuf, currentScript.c_str(), 8191);
      }
      nodeToRename = nullptr;
      renameStarted = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0)) ||
        ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      nodeToRename = nullptr;
      renameStarted = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  } else {
    // If popup closed by clicking outside or other means
    if (renameStarted && !ImGui::IsPopupOpen("RenameNodePopup")) {
      nodeToRename = nullptr;
      renameStarted = false;
    }
  }

  // Save Scene Popup
  if (openSaveScenePopup) {
    ImGui::OpenPopup("SaveScenePopup");
    openSaveScenePopup = false;
  }
  if (ImGui::BeginPopupModal("SaveScenePopup", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename##Scene", sceneFilename, 128);

    RenderSceneFilePicker();
    ImGui::Separator();

    if (ImGui::Button("Save##Scene", ImVec2(120, 0))) {
      std::filesystem::path fullPath = currentPath / sceneFilename;
      SaveScene(fullPath.string());
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel##Scene", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  // Load Scene Popup
  if (openLoadScenePopup) {
    ImGui::OpenPopup("LoadScenePopup");
    openLoadScenePopup = false;
  }
  if (ImGui::BeginPopupModal("LoadScenePopup", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename##SceneLoad", sceneFilename, 128);

    RenderSceneFilePicker();
    ImGui::Separator();

    if (ImGui::Button("Load##SceneLoad", ImVec2(120, 0))) {
      std::filesystem::path fullPath = currentPath / sceneFilename;
      LoadScene(fullPath.string());
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel##SceneLoad", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImVec2 mainWinPos;
  float mainWinWidth;

  if (ImGui::Begin("Logicarium", NULL, window_flags)) {
    mainWinPos = ImGui::GetWindowPos();
    mainWinWidth = ImGui::GetWindowWidth();

    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Scene..", "Ctrl+O")) {
          openLoadScenePopup = true;
        }
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
          openSaveScenePopup = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save Custom Gates...")) {
          openSaveGatePopup = true;
        }
        if (ImGui::MenuItem("Load Custom Gates...")) {
          openLoadGatePopup = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Create Gate..")) {
          openCreateGatePopup = true;
        }
        if (ImGui::MenuItem("Close", "Ctrl+W")) {
        }
        if (ImGui::MenuItem("Force Create Gate (Debug)")) {
          debugMsg = "Force Creating Gate...";
          CreateGate();
          debugMsg += " -> Done";
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Scene Script", "Tab", &showScriptEditor);
        ImGui::MenuItem("Dock", "D", &showDock);
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    // Editing Banner
    if (!editingGateName.empty()) {
      ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(180, 100, 20, 200));
      ImGui::BeginChild("EditingBanner", ImVec2(0, 40), true,
                        ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoScrollWithMouse);
      ImGui::Text("Editing Gate: %s", editingGateName.c_str());
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);
      if (ImGui::Button("Save and Close", ImVec2(120, 0))) {
        UpdateGateDefinitionFromCurrentScene(editingGateName);
        currentScript = originalSceneScript;
        lastParsedScript = ""; // Force reparse
        UpdateNodesFromScript();
        editingGateName = "";
      }
      ImGui::SameLine();
      if (ImGui::Button("Discard", ImVec2(80, 0))) {
        currentScript = originalSceneScript;
        lastParsedScript = ""; // Force reparse
        UpdateNodesFromScript();
        editingGateName = "";
      }
      ImGui::EndChild();
      ImGui::PopStyleColor();
    }

    // Missing Gates Warning Banner
    if (showMissingGatesBanner && !missingGateTypes.empty()) {
      ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(150, 50, 50, 220));
      ImGui::BeginChild("MissingGatesBanner", ImVec2(0, 50), true,
                        ImGuiWindowFlags_NoScrollbar |
                            ImGuiWindowFlags_NoScrollWithMouse);

      // Build list of missing types (truncate if too many)
      std::string typeList;
      for (size_t i = 0; i < missingGateTypes.size() && i < 5; i++) {
        if (i > 0)
          typeList += ", ";
        typeList += missingGateTypes[i];
      }
      if (missingGateTypes.size() > 5) {
        typeList +=
            " (+" + std::to_string(missingGateTypes.size() - 5) + " more)";
      }

      // First line with buttons on right
      float firstLineY = ImGui::GetCursorPosY();
      ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "Warning:");
      ImGui::SameLine();
      ImGui::Text("Scene uses custom gates that are not loaded:");
      ImGui::SameLine(ImGui::GetWindowWidth() - 190);
      ImGui::SetCursorPosY(firstLineY);
      if (ImGui::Button("Load Library...", ImVec2(100, 0))) {
        openLoadGatePopup = true;
      }
      ImGui::SameLine();
      if (ImGui::Button("Dismiss", ImVec2(70, 0))) {
        showMissingGatesBanner = false;
      }

      // Second line: gate list
      ImGui::SetCursorPosY(firstLineY + ImGui::GetTextLineHeightWithSpacing());
      ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "%s",
                         typeList.c_str());

      ImGui::EndChild();
      ImGui::PopStyleColor();
    }

    if (openCreateGatePopup) {
      ImGui::OpenPopup("CreateGatePopup");
      openCreateGatePopup = false;
    }

    if (ImGui::BeginPopupModal("CreateGatePopup", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::InputText("Gate Name", gateName, 128);
      ImGui::ColorEdit3("Color", newGateColor);
      if (ImGui::Button("Create", ImVec2(120, 0))) {
        debugMsg = "Creating Gate: " + std::string(gateName);
        CreateGate();
        debugMsg += " -> Done";
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }

  float sidebarWidth = 400.0f;
  if (showScriptEditor) {
    ImGui::Columns(2, "EditorSplit", true);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() - sidebarWidth);
  }

  ImNodes::Ez::BeginCanvas();
  canvasWindowPos = ImGui::GetWindowPos();

  HandleKeyBindings();

  // --- Cyberpunk Visuals Start ---

  // 1. Hex Grid Background
  auto *canvas = ImNodes::GetCurrentCanvas();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();

  // Draw deep dark background
  drawList->AddRectFilled(
      canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
      IM_COL32(20, 20, 25, 255));

  // Grid Logic
  const float HEX_SIZE = 50.0f * canvas->Zoom;
  const float SQRT3 = 1.7320508f;
  const float HEX_W = HEX_SIZE * SQRT3;
  const float HEX_H = HEX_SIZE * 2.0f;
  const float HEX_X_SPACING = HEX_W;
  const float HEX_Y_SPACING = HEX_H * 0.75f;

  ImVec2 offset = canvas->Offset;

  // Calculate visible range
  int startX = (int)((-offset.x) / HEX_X_SPACING) - 1;
  int startY = (int)((-offset.y) / HEX_Y_SPACING) - 1;
  int endX = startX + (int)(canvasSize.x / HEX_X_SPACING) + 2;
  int endY = startY + (int)(canvasSize.y / HEX_Y_SPACING) + 2;

  ImColor gridColor = IM_COL32(50, 60, 70, 40); // Faint hex lines

  for (int y = startY; y < endY; ++y) {
    for (int x = startX; x < endX; ++x) {
      float xPos = x * HEX_X_SPACING;
      float yPos = y * HEX_Y_SPACING;
      if (y % 2 != 0)
        xPos += HEX_X_SPACING * 0.5f;

      ImVec2 center =
          ImVec2(canvasPos.x + offset.x + xPos, canvasPos.y + offset.y + yPos);

      // Draw Hexagon
      ImVec2 verts[6];
      for (int i = 0; i < 6; ++i) {
        float angle = 3.14159f / 3.0f * (i + 0.5f);
        verts[i] = ImVec2(center.x + cos(angle) * HEX_SIZE * 0.5f,
                          center.y + sin(angle) * HEX_SIZE * 0.5f);
      }
      drawList->AddPolyline(verts, 6, gridColor, true, 1.5f);
    }
  }

  // 2. Glassmorphism Styles
  ImNodes::Ez::PushStyleVar(ImNodesStyleVar_NodeRounding, 8.0f);
  ImNodes::Ez::PushStyleVar(ImNodesStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
  ImNodes::Ez::PushStyleVar(ImNodesStyleVar_NodeSpacing, ImVec2(4.0f, 4.0f));

  // Update rendering colors in RenderNode manually? No, we set them per node
  // locally mostly, but lets set defaults for others
  canvas->Colors[ImNodes::ColCanvasLines] =
      IM_COL32(0, 0, 0, 0); // Hide default grid
  canvas->Colors[ImNodes::ColNodeBorder] =
      IM_COL32(100, 200, 255, 150); // Cyan glass border

  ImGui::Text("Debug: %s", debugMsg.c_str());

  RenderNodes();

  RenderContextMenu();

  // Detect connection dropped in empty space
  if (!showConnectionDropMenu) {
    void *srcNode;
    const char *srcSlot;
    int srcKind;
    if (ImNodes::GetPendingConnection(&srcNode, &srcSlot, &srcKind)) {
      const ImGuiPayload *payload = ImGui::GetDragDropPayload();
      if (payload && ImGui::IsMouseReleased(0) && !payload->Delivery) {
        dropSourceNode = srcNode;
        dropSourceSlot = std::string(srcSlot);
        dropSourceSlotKind = srcKind;
        connectionDropPos = ImGui::GetMousePos();

        bool isInput = ImNodes::IsInputSlotKind(srcKind);
        ImNodes::GetSlotPosition(srcNode, srcSlot, isInput,
                                 &connectionSourceSlotPos);

        showConnectionDropMenu = true;
        ImGui::OpenPopup("ConnectionDropMenu");
      }
    }
  }

  RenderConnectionDropMenu();

  RenderDock();

  ImNodes::Ez::EndCanvas();
  ImNodes::Ez::PopStyleVar(3);

  if (showScriptEditor) {
    ImGui::NextColumn();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::BeginChild("ScriptPanel", ImVec2(0, 0), false);

    // Header Row
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    ImGui::Text("Scene Script");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 90);
    if (ImGui::SmallButton("Sync")) {
      UpdateScriptFromNodes();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Apply")) {
      lastParsedScript = ""; // Force reparse
      UpdateNodesFromScript();
    }
    ImGui::PopStyleVar(); // FramePadding

    // Only update script from nodes if the user isn't currently typing in the
    // editor
    static bool scriptActive = false;

    if (!scriptActive) {
      UpdateScriptFromNodes();
    }

    // scriptBuf is now a member variable
    static bool bufferInitialized = false;
    if (!scriptActive || !bufferInitialized) {
      strncpy(scriptBuf, currentScript.c_str(), 8191);
      bufferInitialized = true;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
    if (ImGui::InputTextMultiline(
            "##script", scriptBuf, 8192,
            ImVec2(-1, -ImGui::GetTextLineHeightWithSpacing() * 8))) {
      currentScript = scriptBuf;
      UpdateNodesFromScript();
    }
    ImGui::PopStyleVar();

    scriptActive = ImGui::IsItemActive();

    if (!scriptError.empty()) {
      ImGui::Separator();
      if (ImGui::Selectable(errorPanelCollapsed ? "> Show Errors"
                                                : "v Hide Errors")) {
        errorPanelCollapsed = !errorPanelCollapsed;
      }
      if (!errorPanelCollapsed) {
        ImGui::BeginChild("ErrorList", ImVec2(0, 150), true);
        ImGui::SetWindowFontScale(0.9f);
        ImGui::TextWrapped("%s", scriptError.c_str());
        ImGui::EndChild();
      }
    } // End of !scriptError.empty()

    // AI Assistant Section
    RenderAIAssistant();

    ImGui::EndChild();
    ImGui::PopStyleVar(2); // ItemSpacing, WindowPadding
    ImGui::Columns(1);
  }

  ImGui::End();

  // Persistent Sidebar Toggle Overlay
  float buttonWidth = 30.0f;
  float overlayX = mainWinPos.x + mainWinWidth - buttonWidth -
                   5; // Default (Collapsed) position

  if (showScriptEditor) {
    overlayX = mainWinPos.x + mainWinWidth - sidebarWidth - buttonWidth;
  }

  ImGui::SetNextWindowPos(ImVec2(overlayX, mainWinPos.y + 300));
  ImGui::SetNextWindowBgAlpha(0.0f);
  if (ImGui::Begin(
          "SidebarToggleOverlay", NULL,
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
              ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
    if (ImGui::Button(showScriptEditor ? ">" : "<", ImVec2(buttonWidth, 30))) {
      showScriptEditor = !showScriptEditor;
    }
  }
  ImGui::End();

  // Logic Editor Modal
  if (showCodeEditor) {
    ImGui::OpenPopup("Logic Editor");
    showCodeEditor = false;
  }

  if (ImGui::BeginPopupModal("Logic Editor", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Editing Logic for: %p (%s)", gateBeingEdited,
                gateBeingEdited ? gateBeingEdited->title : "Unknown");
    ImGui::Separator();

    char codeBuf[1024];
    strncpy(codeBuf, editingCode.c_str(), 1023);
    if (ImGui::InputTextMultiline("##gateCode", codeBuf, 1024,
                                  ImVec2(400, 200))) {
      editingCode = codeBuf;
    }

    ImGui::Separator();
    if (ImGui::Button("Apply", ImVec2(120, 0))) {
      if (gateBeingEdited) {
        gateBeingEdited->SetCode(editingCode);
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void NodeEditor::RenderConnectionDropMenu() {
  if (!showConnectionDropMenu)
    return;

  // Draw connection line from source slot to drop position
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  auto *canvas = ImNodes::GetCurrentCanvas();
  float thickness = canvas->Style.CurveThickness * canvas->Zoom;
  float dx = ImFabs(connectionSourceSlotPos.x - connectionDropPos.x);
  float strength = ImMin(canvas->Style.CurveStrength * canvas->Zoom, dx * 0.5f);

  ImVec2 input_pos, output_pos;
  float indent = canvas->Style.ConnectionIndent * canvas->Zoom;

  if (ImNodes::IsInputSlotKind(dropSourceSlotKind)) {
    input_pos = connectionSourceSlotPos;
    input_pos.x += indent;
    output_pos = connectionDropPos;
  } else {
    input_pos = connectionDropPos;
    output_pos = connectionSourceSlotPos;
    output_pos.x -= indent;
  }

  ImVec2 p2 = input_pos - ImVec2{strength, 0};
  ImVec2 p3 = output_pos + ImVec2{strength, 0};
  drawList->AddBezierCubic(input_pos, p2, p3, output_pos,
                           IM_COL32(200, 200, 200, 150), thickness);

  // Render popup menu
  ImGui::SetNextWindowPos(connectionDropPos);
  if (ImGui::BeginPopup("ConnectionDropMenu")) {
    bool fromOutput = ImNodes::IsOutputSlotKind(dropSourceSlotKind);

    for (const auto &desc : availableNodes) {
      auto item = desc();
      // Filter: if dragging from output, only show nodes with inputs
      // If dragging from input, only show nodes with outputs
      bool compatible =
          fromOutput ? (item->inputSlotCount > 0) : (item->outputSlotCount > 0);
      if (compatible && ImGui::MenuItem(item->title)) {
        Node *newNode = desc();

        // Position node at drop location (canvas coordinates)
        newNode->pos = (connectionDropPos - canvasWindowPos) / canvas->Zoom -
                       canvas->Offset;
        nodes.push_back(newNode);

        // Create connection
        Connection conn;
        if (fromOutput) {
          conn.outputNode = dropSourceNode;
          conn.outputSlot = dropSourceSlot;
          conn.inputNode = newNode;
          conn.inputSlot = newNode->inputSlots[0].title;
        } else {
          // Input pins only accept single connection - remove existing one
          Node *inputNode = (Node *)dropSourceNode;
          for (auto it = inputNode->connections.begin();
               it != inputNode->connections.end(); ++it) {
            if (it->inputNode == dropSourceNode &&
                it->inputSlot == dropSourceSlot) {
              ((Node *)it->outputNode)->DeleteConnection(*it);
              inputNode->connections.erase(it);
              break;
            }
          }

          conn.inputNode = dropSourceNode;
          conn.inputSlot = dropSourceSlot;
          conn.outputNode = newNode;
          conn.outputSlot = newNode->outputSlots[0].title;
        }

        ((Node *)conn.inputNode)->connections.push_back(conn);
        ((Node *)conn.outputNode)->connections.push_back(conn);

        showConnectionDropMenu = false;
      }
      delete item;
    }

    ImGui::Separator();
    if (ImGui::BeginMenu("Gates")) {
      for (const auto &desc : availableGates) {
        auto item = desc();
        bool compatible = fromOutput ? (item->inputSlotCount > 0)
                                     : (item->outputSlotCount > 0);
        if (compatible && ImGui::MenuItem(item->title)) {
          Node *newNode = desc();
          newNode->pos = (connectionDropPos - canvasWindowPos) / canvas->Zoom -
                         canvas->Offset;
          nodes.push_back(newNode);

          Connection conn;
          if (fromOutput) {
            conn.outputNode = dropSourceNode;
            conn.outputSlot = dropSourceSlot;
            conn.inputNode = newNode;
            conn.inputSlot = newNode->inputSlots[0].title;
          } else {
            // Input pins only accept single connection - remove existing one
            Node *inputNode = (Node *)dropSourceNode;
            for (auto it = inputNode->connections.begin();
                 it != inputNode->connections.end(); ++it) {
              if (it->inputNode == dropSourceNode &&
                  it->inputSlot == dropSourceSlot) {
                ((Node *)it->outputNode)->DeleteConnection(*it);
                inputNode->connections.erase(it);
                break;
              }
            }

            conn.inputNode = dropSourceNode;
            conn.inputSlot = dropSourceSlot;
            conn.outputNode = newNode;
            conn.outputSlot = newNode->outputSlots[0].title;
          }

          ((Node *)conn.inputNode)->connections.push_back(conn);
          ((Node *)conn.outputNode)->connections.push_back(conn);

          showConnectionDropMenu = false;
        }
        delete item;
      }
      ImGui::EndMenu();
    }

    ImGui::EndPopup();
  } else {
    // Popup was closed (clicked outside)
    showConnectionDropMenu = false;
  }
}

NodeEditor::NodeEditor() {
  // Initialize AI Assistant
  aiAssistant = std::make_unique<AIAssistant>();
  aiAssistant->LoadConfig("ai_config.json");
}

NodeEditor::~NodeEditor() {
  // Destructor needed for unique_ptr<AIAssistant> with forward declaration
}
} // namespace Logicarium
