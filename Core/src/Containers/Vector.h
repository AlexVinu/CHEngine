#pragma once

#include "Memory/MemorySystem.h"
#include <cassert>
#include <utility>
#include <initializer_list>
#include <new>

namespace CHEngine
{
    template<typename T>
    class Vector
    {
    public:
        using value_type = T;
        using iterator = T*;
        using const_iterator = const T*;

        // -------------------------------------------------------
        // Constructors
        // -------------------------------------------------------

        explicit Vector(IAllocator* allocator = MemorySystem::GetAllocator())
            : m_Data(nullptr),
            m_Size(0),
            m_Capacity(0),
            m_Allocator(allocator)
        {
        }

        Vector(size_t count, const T& value,
            IAllocator* allocator = MemorySystem::GetAllocator())
            : Vector(allocator)
        {
            resize(count, value);
        }

        Vector(std::initializer_list<T> list,
            IAllocator* allocator = MemorySystem::GetAllocator())
            : Vector(allocator)
        {
            reserve(list.size());
            for (const auto& v : list)
                emplace_back(v);
        }

        // Copy
        Vector(const Vector& other)
            : Vector(other.m_Allocator)
        {
            reserve(other.m_Size);
            for (size_t i = 0; i < other.m_Size; ++i)
                emplace_back(other.m_Data[i]);
        }

        // Move
        Vector(Vector&& other) noexcept
            : m_Data(other.m_Data),
            m_Size(other.m_Size),
            m_Capacity(other.m_Capacity),
            m_Allocator(other.m_Allocator)
        {
            other.m_Data = nullptr;
            other.m_Size = 0;
            other.m_Capacity = 0;
        }

        // Destructor
        ~Vector()
        {
            clear();
            if (m_Data)
                m_Allocator->Free(m_Data);
        }

        // -------------------------------------------------------
        // Assignment
        // -------------------------------------------------------

        Vector& operator=(const Vector& other)
        {
            if (this == &other) return *this;

            clear();
            reserve(other.m_Size);

            for (size_t i = 0; i < other.m_Size; ++i)
                emplace_back(other.m_Data[i]);

            return *this;
        }

        Vector& operator=(Vector&& other) noexcept
        {
            if (this == &other) return *this;

            clear();
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

        // -------------------------------------------------------
        // Capacity
        // -------------------------------------------------------

        size_t size() const { return m_Size; }
        size_t capacity() const { return m_Capacity; }
        bool empty() const { return m_Size == 0; }

        void reserve(size_t newCapacity)
        {
            if (newCapacity <= m_Capacity)
                return;

            reallocate(newCapacity);
        }

        void resize(size_t newSize)
        {
            if (newSize < m_Size)
            {
                for (size_t i = newSize; i < m_Size; ++i)
                    m_Data[i].~T();
            }
            else if (newSize > m_Size)
            {
                reserve(newSize);
                for (size_t i = m_Size; i < newSize; ++i)
                    new (&m_Data[i]) T();
            }

            m_Size = newSize;
        }

        void resize(size_t newSize, const T& value)
        {
            size_t oldSize = m_Size;
            resize(newSize);

            for (size_t i = oldSize; i < newSize; ++i)
                m_Data[i] = value;
        }

        void shrink_to_fit()
        {
            if (m_Size < m_Capacity)
                reallocate(m_Size);
        }

        // -------------------------------------------------------
        // Element access
        // -------------------------------------------------------

        T& operator[](size_t index)
        {
            assert(index < m_Size);
            return m_Data[index];
        }

        const T& operator[](size_t index) const
        {
            assert(index < m_Size);
            return m_Data[index];
        }

        T& at(size_t index)
        {
            if (index >= m_Size)
                throw std::out_of_range("Vector::at");
            return m_Data[index];
        }

        T& front() { return m_Data[0]; }
        T& back() { return m_Data[m_Size - 1]; }

        const T& front() const { return m_Data[0]; }
        const T& back() const { return m_Data[m_Size - 1]; }

        T* data() { return m_Data; }
        const T* data() const { return m_Data; }

        // -------------------------------------------------------
        // Modifiers
        // -------------------------------------------------------

        template<typename... Args>
        T& emplace_back(Args&&... args)
        {
            ensure_capacity(m_Size + 1);

            new (&m_Data[m_Size]) T(std::forward<Args>(args)...);
            return m_Data[m_Size++];
        }

        void push_back(const T& value)
        {
            emplace_back(value);
        }

        void push_back(T&& value)
        {
            emplace_back(std::move(value));
        }

        void pop_back()
        {
            assert(m_Size > 0);
            m_Data[--m_Size].~T();
        }

        void clear()
        {
            for (size_t i = 0; i < m_Size; ++i)
                m_Data[i].~T();
            m_Size = 0;
        }

        // -------------------------------------------------------
        // Iterators
        // -------------------------------------------------------

        iterator begin() { return m_Data; }
        iterator end() { return m_Data + m_Size; }

        const_iterator begin() const { return m_Data; }
        const_iterator end() const { return m_Data + m_Size; }

    private:

        void ensure_capacity(size_t minCapacity)
        {
            if (minCapacity <= m_Capacity)
                return;

            size_t newCapacity = m_Capacity ? m_Capacity * 2 : 4;
            if (newCapacity < minCapacity)
                newCapacity = minCapacity;

            reallocate(newCapacity);
        }

        void reallocate(size_t newCapacity)
        {
            T* newData = static_cast<T*>(
                m_Allocator->Allocate(sizeof(T) * newCapacity, alignof(T))
                );

            for (size_t i = 0; i < m_Size; ++i)
                new (&newData[i]) T(std::move(m_Data[i]));

            for (size_t i = 0; i < m_Size; ++i)
                m_Data[i].~T();

            if (m_Data)
                m_Allocator->Free(m_Data);

            m_Data = newData;
            m_Capacity = newCapacity;
        }

    private:
        T* m_Data;
        size_t m_Size;
        size_t m_Capacity;
        IAllocator* m_Allocator;
    };
}