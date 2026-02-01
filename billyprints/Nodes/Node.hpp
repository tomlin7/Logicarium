#pragma once

#include "Connection.hpp"
#include "pch.hpp"
#include <string>

namespace Billyprints {
class Node {
public:
  /// Node title
  const char *title = nullptr;
  std::string id = "";
  bool selected = false;
  ImVec2 pos{};
  bool value = false;
  uint64_t lastEvaluatedFrame = 0;
  bool isEvaluating = false;
  static uint64_t GlobalFrameCount;

  std::vector<Connection> connections{};
  std::vector<ImNodes::Ez::SlotInfo> inputSlots{};
  std::vector<ImNodes::Ez::SlotInfo> outputSlots{};

  int inputSlotCount;
  int outputSlotCount;

  Node(const char *title, std::vector<ImNodes::Ez::SlotInfo> &&_inputSlots,
       std::vector<ImNodes::Ez::SlotInfo> &&_outputSlots);
  void DeleteConnection(const Connection &connection);
  virtual ~Node() = default;
  virtual bool Evaluate(const std::string &slot = "");
  virtual void Render();
  virtual ImU32 GetColor() const;
};
} // namespace Billyprints