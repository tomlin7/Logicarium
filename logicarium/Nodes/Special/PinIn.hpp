#pragma once

#include "Node.hpp"

namespace Logicarium {
class PinIn : public Node {
public:
  PinIn();
  bool Evaluate(const std::string &slot = "") override;
  void Render() override;
  bool isMomentary = false;
  ImU32 GetColor() const override { return IM_COL32(40, 40, 45, 255); }
};
} // namespace Logicarium
