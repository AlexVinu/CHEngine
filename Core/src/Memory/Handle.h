#pragma once

#include <cstdint>

namespace CHEngine {

	template<typename Tag>
	struct Handle
	{
		static constexpr uint32_t INVALID_INDEX = 0xFFFFFFFF;

		uint32_t index      = INVALID_INDEX;
		uint32_t generation = 0;

		bool IsValid() const { return index != INVALID_INDEX; }

		static Handle Invalid() { return {}; }

		bool operator==(const Handle& other) const
		{
			return index == other.index && generation == other.generation;
		}

		bool operator!=(const Handle& other) const
		{
			return !(*this == other);
		}
	};

}
