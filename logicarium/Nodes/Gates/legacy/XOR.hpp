#pragma once

#include "Gate.hpp"

namespace Logicarium {
	class XOR : public Gate
	{
	public:
		XOR();
		static bool XOR_F(const std::vector<bool>& input, const int&);

		bool Evaluate() override;
	};
}
