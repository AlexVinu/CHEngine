#pragma once

#include <bit>
#include <concepts>
#include <cstdint>
#include <type_traits>


namespace CHEngine
{
    template<typename TEnum, typename TStore = std::underlying_type_t<TEnum>>
        requires std::is_enum_v<TEnum>&& std::unsigned_integral<TStore>
    class BitwiseRange
    {
    public:
        using EnumType = TEnum;
        using StoreType = TStore;

        class Iterator
        {
        public:
            constexpr explicit Iterator(StoreType remainingBits = 0)
                : m_Remaining(remainingBits)
            {
            }

            constexpr EnumType operator*() const
            {
                return static_cast<EnumType>(std::countr_zero(m_Remaining));
            }

            constexpr Iterator& operator++()
            {
                m_Remaining &= (m_Remaining - 1);
                return *this;
            }

            constexpr bool operator==(const Iterator& other) const
            {
                return m_Remaining == other.m_Remaining;
            }

            constexpr bool operator!=(const Iterator& other) const
            {
                return !(*this == other);
            }

        private:
            StoreType m_Remaining;
        };

    public:
        constexpr explicit BitwiseRange(StoreType bits)
            : m_Bits(bits)
        {
        }

        constexpr Iterator begin() const
        {
            return Iterator{ m_Bits };
        }

        constexpr Iterator end() const
        {
            return Iterator{ 0 };
        }

    private:
        StoreType m_Bits;
    };
}