#pragma once

#include "Node.hpp"
#include "pch.hpp"

namespace Logicarium {
class Gate : public Node {
public:
  Gate(const char *title, std::vector<ImNodes::Ez::SlotInfo> &&inputSlots,
       std::vector<ImNodes::Ez::SlotInfo> &&outputSlots);

  virtual void Render() override;
  virtual ImU32 GetColor() const override;

  virtual std::string GetCode() const { return logicCode; }
  virtual void SetCode(const std::string &code) { logicCode = code; }

protected:
  std::string logicCode;
  bool EvaluateExpression();
};
} // namespace Logicarium
