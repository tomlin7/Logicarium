#pragma once

#include "Gate.hpp"
#include "AND.hpp"

namespace Logicarium {
	class NAND : public Gate
	{
	public:
		NAND();
		static bool NAND_F(const std::vector<bool>& input, const int& pinCount);

		bool Evaluate() override;
	};
}
