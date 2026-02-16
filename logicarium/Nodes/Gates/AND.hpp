#pragma once

#include "Gate.hpp"

namespace Logicarium {
class AND : public Gate {
public:
  AND();
  static bool AND_F(const std::vector<bool> &input, const int &pinCount);

  bool Evaluate(const std::string &slot = "") override;
};
} // namespace Logicarium
