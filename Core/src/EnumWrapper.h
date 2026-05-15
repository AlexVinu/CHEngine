#pragma once

#include <CheStl/String.h>

namespace CHEngine {
    template<typename TEnum, auto FnToString> 
        requires std::is_enum_v<TEnum>&&
            std::is_invocable_r_v<String, decltype(FnToString), TEnum>
    class EnumWrapper
    {
    public:
        using EnumType = TEnum;

        EnumWrapper(EnumType val)
            : m_Value(val)
        {
            m_Name = FnToString(val);
        }
        const char* c_str() { return m_Name.c_str(); }
        EnumType GetEnum() { return m_Value; }
        operator int() const { return static_cast<int>(m_Value); }
    private:
        EnumType m_Value;
        String m_Name;
    };
}
