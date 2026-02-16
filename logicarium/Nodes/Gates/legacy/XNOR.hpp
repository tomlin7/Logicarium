#pragma once

#include "Gate.hpp"
#include "XOR.hpp"

namespace Logicarium {
	class XNOR : public Gate
	{
	public:
		XNOR();
		static bool XNOR_F(const std::vector<bool>& input, const int& pinCount);

		bool Evaluate() override;
	};
}
