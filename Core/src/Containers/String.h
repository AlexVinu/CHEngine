#pragma once
#include "Memory/MemorySystem.h"
#include <cstring>
#include <cassert>

namespace CHEngine
{

    class String
    {
    private:

        static constexpr size_t SSO_SIZE = 23;
        static constexpr size_t SSO_FLAG = 1;

    public:

        String(IAllocator* allocator = MemorySystem::GetAllocator())
            : m_Size(0), m_Capacity(SSO_SIZE | SSO_FLAG), m_Allocator(allocator)
        {
            m_Data.small[0] = '\0';
        }

        String(const char* str, IAllocator* allocator = MemorySystem::GetAllocator())
            : String(allocator)
        {
            append(str);
        }

        String(const String& other)
            : String(other.m_Allocator)
        {
            append(other.c_str());
        }

        String(String&& other) noexcept
            : m_Size(other.m_Size),
            m_Capacity(other.m_Capacity),
            m_Allocator(other.m_Allocator)
        {
            if (other.is_sso())
            {
                std::memcpy(m_Data.small, other.m_Data.small, other.m_Size + 1);
            }
            else
            {
                m_Data.heap = other.m_Data.heap;
                other.m_Data.heap = nullptr;
            }

            other.m_Size = 0;
            other.m_Capacity = SSO_SIZE | SSO_FLAG;
        }

        ~String()
        {
            if (!is_sso() && m_Data.heap)
                m_Allocator->Free(m_Data.heap);
        }

    public:

        String& operator=(const String& other)
        {
            if (this == &other)
                return *this;

            clear();
            m_Allocator = other.m_Allocator;
            append(other.c_str());

            return *this;
        }

        String& operator=(String&& other) noexcept
        {
            if (this == &other)
                return *this;

            if (!is_sso())
                m_Allocator->Free(m_Data.heap);

            m_Size = other.m_Size;
            m_Capacity = other.m_Capacity;
            m_Allocator = other.m_Allocator;

            if (other.is_sso())
            {
                std::memcpy(m_Data.small, other.m_Data.small, other.m_Size + 1);
            }
            else
            {
                m_Data.heap = other.m_Data.heap;
                other.m_Data.heap = nullptr;
            }

            other.m_Size = 0;
            other.m_Capacity = SSO_SIZE | SSO_FLAG;

            return *this;
        }

    public:

        void append(const char* str)
        {
            if (!str)
                return;

            size_t len = std::strlen(str);
            reserve(m_Size + len + 1);

            char* dst = data();

            std::memcpy(dst + m_Size, str, len);
            m_Size += len;
            dst[m_Size] = '\0';
        }

        void append(const String& other)
        {
            append(other.c_str());
        }

        String& operator+=(const String& other)
        {
            append(other);
            return *this;
        }

        friend String operator+(const String& a, const String& b)
        {
            String r(a);
            r += b;
            return r;
        }

    public:

        void reserve(size_t capacity)
        {
            if (capacity <= get_capacity())
                return;

            size_t newCap = get_capacity() * 2;
            if (newCap < capacity)
                newCap = capacity;

            char* newData = static_cast<char*>(m_Allocator->Allocate(newCap, alignof(char)));

            std::memcpy(newData, c_str(), m_Size + 1);

            if (!is_sso())
                m_Allocator->Free(m_Data.heap);

            m_Data.heap = newData;
            m_Capacity = newCap & ~SSO_FLAG;
        }

        void clear()
        {
            m_Size = 0;
            data()[0] = '\0';
        }

    public:

        const char* c_str() const
        {
            return is_sso() ? m_Data.small : m_Data.heap;
        }

        char* data()
        {
            return is_sso() ? m_Data.small : m_Data.heap;
        }

        const char* data() const
        {
            return c_str();
        }

        size_t size() const
        {
            return m_Size;
        }

        size_t capacity() const
        {
            return get_capacity();
        }

    private:

        bool is_sso() const
        {
            return (m_Capacity & SSO_FLAG) != 0;
        }

        size_t get_capacity() const
        {
            return m_Capacity & ~SSO_FLAG;
        }

    private:

        union
        {
            char* heap;
            char small[SSO_SIZE + 1];
        } m_Data;

        size_t m_Size;
        size_t m_Capacity;
        IAllocator* m_Allocator;
    };

}