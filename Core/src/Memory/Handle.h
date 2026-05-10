#pragma once

#include <cstdint>
#include <functional>

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

		// Special cast for specific cases when it needed to treat handle like handle<void>
		template<typename OtherTag = void>
			requires std::is_same_v<Tag, void> || std::is_same_v<OtherTag, void>
		operator Handle<OtherTag>() const
		{
			return Handle<OtherTag>{index, generation};
		}
	};

	// ADL hook for boost::hash (used by boost::bimap unordered_set_of).
	template<typename Tag>
	inline std::size_t hash_value(const Handle<Tag>& h) noexcept
	{
		return std::hash<Handle<Tag>>{}(h);
	}

}

namespace std {
	template<typename Tag>
	struct hash<CHEngine::Handle<Tag>>
	{
		size_t operator()(const CHEngine::Handle<Tag>& h) const noexcept
		{
			return std::hash<uint64_t>{}(
				(static_cast<uint64_t>(h.index) << 32) | h.generation
			);
		}
	};
}
