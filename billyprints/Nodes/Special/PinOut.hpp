#pragma once

#include "Node.hpp"

namespace Billyprints {
class PinOut : public Node {
public:
  PinOut();
  bool Evaluate(const std::string &slot = "") override;
  void Render() override;
  ImU32 GetColor() const override { return IM_COL32(40, 40, 45, 255); }
};
} // namespace Billyprints
