#pragma once

#include "Gate.hpp"
#include <string>

namespace Billyprints {

// PlaceholderGate represents a custom gate that is not currently loaded.
// It preserves the gate's type name and slot configuration so that:
// 1. Connections can be maintained even when the gate definition is missing
// 2. The scene can be saved and reloaded correctly
// 3. The gate can be "upgraded" to a real CustomGate when the library is loaded
class PlaceholderGate : public Gate {
public:
  PlaceholderGate(const std::string &missingTypeName, int inputs, int outputs);
  ~PlaceholderGate();

  bool Evaluate() override;
  void Render() override;
  ImU32 GetColor() const override;

  // The original type name (e.g., "HalfAdder") for later upgrade
  std::string missingTypeName;
};

} // namespace Billyprints
