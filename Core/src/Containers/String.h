#pragma once
#include "Memory/Memory.h"
#include "Memory/MemorySystem.h"
#include <cstring>
#include <cassert>
#include <utility>

namespace CHEngine
{
    class String
    {
    public:
        // Конструкторы
        String(IAllocator* allocator = MemorySystem::GetAllocator())
            : m_Data(nullptr), m_Size(0), m_Capacity(0), m_Allocator(allocator) {
        }

        String(const char* cstr, IAllocator* allocator = MemorySystem::GetAllocator())
            : String(allocator)
        {
            append(cstr);
        }

        // Copy constructor
        String(const String& other)
            : m_Allocator(other.m_Allocator)
        {
            m_Size = other.m_Size;
            m_Capacity = other.m_Size + 1;
            m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity, alignof(char)));
            std::memcpy(m_Data, other.m_Data, m_Size + 1);
        }

        // Move constructor
        String(String&& other) noexcept
            : m_Data(other.m_Data), m_Size(other.m_Size),
            m_Capacity(other.m_Capacity), m_Allocator(other.m_Allocator)
        {
            other.m_Data = nullptr;
            other.m_Size = 0;
            other.m_Capacity = 0;
        }

        // Destructor
        ~String()
        {
            if (m_Data)
                m_Allocator->Free(m_Data);
        }

        // Copy assignment
        String& operator=(const String& other)
        {
            if (this == &other) return *this;

            if (m_Data)
                m_Allocator->Free(m_Data);

            m_Allocator = other.m_Allocator;
            m_Size = other.m_Size;
            m_Capacity = other.m_Size + 1;
            m_Data = static_cast<char*>(m_Allocator->Allocate(m_Capacity, alignof(char)));
            std::memcpy(m_Data, other.m_Data, m_Size + 1);

            return *this;
        }

        // Move assignment
        String& operator=(String&& other) noexcept
        {
            if (this == &other) return *this;

            if (m_Data)
                m_Allocator->Free(m_Data);

            m_Data = other.m_Data;
            m_Size = other.m_Size;
            m_Capacity = other.m_Capacity;
            m_Allocator = other.m_Allocator;

            other.m_Data = nullptr;
            other.m_Size = 0;
            other.m_Capacity = 0;

            return *this;
        }

        // Append C-string
        void append(const char* cstr)
        {
            size_t len = std::strlen(cstr);
            ensure_capacity(m_Size + len + 1);

            std::memcpy(m_Data + m_Size, cstr, len);
            m_Size += len;
            m_Data[m_Size] = '\0';
        }

        // Append another String
        void append(const String& other)
        {
            append(other.c_str());
        }

        // Operator+=
        String& operator+=(const String& other)
        {
            append(other);
            return *this;
        }

        // Operator+
        friend String operator+(const String& a, const String& b)
        {
            String result(a);
            result += b;
            return result;
        }

        const char* c_str() const { return m_Data ? m_Data : ""; }
        size_t size() const { return m_Size; }

    private:
        void ensure_capacity(size_t newCapacity)
        {
            if (newCapacity <= m_Capacity) return;

            size_t cap = m_Capacity ? m_Capacity * 2 : 16;
            while (cap < newCapacity) cap *= 2;

            char* newData = static_cast<char*>(m_Allocator->Allocate(cap, alignof(char)));

            if (m_Data)
            {
                if (m_Size)
                    std::memcpy(newData, m_Data, m_Size);
                m_Allocator->Free(m_Data);
            }

            m_Data = newData;
            m_Capacity = cap;
        }

    private:
        char* m_Data;
        size_t m_Size;
        size_t m_Capacity;
        IAllocator* m_Allocator;
    };
}