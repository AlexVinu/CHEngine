#pragma once

#include "Handle.h"
#include <vector>
#include <functional>
#include <cstdint>

namespace CHEngine {

	template<typename T, typename Tag>
	class HandlePool
	{
	public:
		using HandleType = Handle<Tag>;
		using DeleterFn  = std::function<void(T*)>;

		HandlePool() = default;

		explicit HandlePool(DeleterFn deleter)
			: m_Deleter(std::move(deleter))
		{
		}

		~HandlePool()
		{
			Clear();
		}

		HandlePool(const HandlePool&) = delete;
		HandlePool& operator=(const HandlePool&) = delete;

		HandlePool(HandlePool&& other) noexcept
			: m_Slots(std::move(other.m_Slots))
			, m_FreeList(std::move(other.m_FreeList))
			, m_Deleter(std::move(other.m_Deleter))
			, m_Count(other.m_Count)
		{
			other.m_Count = 0;
		}

		HandlePool& operator=(HandlePool&& other) noexcept
		{
			if (this != &other)
			{
				Clear();
				m_Slots    = std::move(other.m_Slots);
				m_FreeList = std::move(other.m_FreeList);
				m_Deleter  = std::move(other.m_Deleter);
				m_Count    = other.m_Count;
				other.m_Count = 0;
			}
			return *this;
		}

		HandleType Add(T* ptr)
		{
			if (!m_FreeList.empty())
			{
				uint32_t idx = m_FreeList.back();
				m_FreeList.pop_back();

				auto& slot    = m_Slots[idx];
				slot.ptr      = ptr;
				slot.occupied = true;
				++m_Count;

				return { idx, slot.generation };
			}

			uint32_t idx = static_cast<uint32_t>(m_Slots.size());
			m_Slots.push_back({ ptr, 0, true });
			++m_Count;

			return { idx, 0 };
		}

		T* Get(HandleType h) const
		{
			if (h.index >= m_Slots.size())
				return nullptr;

			const auto& slot = m_Slots[h.index];
			if (!slot.occupied || slot.generation != h.generation)
				return nullptr;

			return slot.ptr;
		}

		void Remove(HandleType h)
		{
			if (h.index >= m_Slots.size())
				return;

			auto& slot = m_Slots[h.index];
			if (!slot.occupied || slot.generation != h.generation)
				return;

			if (m_Deleter && slot.ptr)
				m_Deleter(slot.ptr);

			slot.ptr      = nullptr;
			slot.occupied = false;
			++slot.generation;
			--m_Count;

			m_FreeList.push_back(h.index);
		}

		void Clear()
		{
			m_FreeList.clear();

			for (uint32_t i = 0; i < m_Slots.size(); ++i)
			{
				auto& slot = m_Slots[i];

				if (slot.occupied && slot.ptr && m_Deleter)
					m_Deleter(slot.ptr);

				if (slot.occupied)
					++slot.generation;

				slot.ptr = nullptr;
				slot.occupied = false;
				m_FreeList.push_back(i);
			}

			m_Count = 0;
		}

		uint32_t Count() const { return m_Count; }
		bool     Empty() const { return m_Count == 0; }

		template<typename Fn>
		void ForEachOccupied(Fn&& fn)
		{
			for (auto& slot : m_Slots)
			{
				if (!slot.occupied || !slot.ptr)
					continue;

				fn(slot.ptr);
			}
		}

		template<typename Fn>
		void ForEachOccupied(Fn&& fn) const
		{
			for (const auto& slot : m_Slots)
			{
				if (!slot.occupied || !slot.ptr)
					continue;

				fn(slot.ptr);
			}
		}

	private:
		struct Slot
		{
			T*       ptr         = nullptr;
			uint32_t  generation = 0;
			bool     occupied    = false;
		};

		std::vector<Slot>     m_Slots;
		std::vector<uint32_t> m_FreeList;
		DeleterFn             m_Deleter;
		uint32_t              m_Count = 0;
	};

}
