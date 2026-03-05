#pragma once

#include <cstdint>

namespace CHEngine {

	template<typename Tag>
	struct Handle
	{
		static constexpr uint64_t INVALID_INDEX = 0xFFFFFFFF;

		uint64_t index      :  32 = INVALID_INDEX;
		uint64_t generation :  32 = 0;

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
