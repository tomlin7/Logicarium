#pragma once

#include "Gate.hpp"

namespace Logicarium {
	class Buffer : public Gate
	{
	public:
		Buffer();
		static bool Buffer_F(const std::vector<bool>& input, const int&);

		bool Evaluate() override;
	};
}
