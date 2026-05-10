#pragma once

#include <stdint.h>
#include <cstring>
#include <utility>

#include "MemorySystem.h"

namespace CHEngine {

	// Owning raw buffer; move-only — destructor frees via the engine allocator.
	class Buffer
	{
	public:
		uint8_t* Data = nullptr;
		uint64_t Size = 0;

		Buffer() = default;

		explicit Buffer(uint64_t size)
		{
			Allocate(size);
		}

		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		Buffer(Buffer&& other) noexcept
			: Data(other.Data)
			, Size(other.Size)
			, m_Allocator(other.m_Allocator)
		{
			other.Data = nullptr;
			other.Size = 0;
		}

		Buffer& operator=(Buffer&& other) noexcept
		{
			if (this == &other)
				return *this;
			Release();
			Data = other.Data;
			Size = other.Size;
			m_Allocator = other.m_Allocator;
			other.Data = nullptr;
			other.Size = 0;
			return *this;
		}

		~Buffer()
		{
			Release();
		}

		static Buffer Copy(const Buffer& other)
		{
			if (!other.Data || other.Size == 0)
				return {};
			Buffer result(other.Size);
			memcpy(result.Data, other.Data, static_cast<size_t>(other.Size));
			return result;
		}

		void Allocate(uint64_t size)
		{
			Release();

			Data = static_cast<uint8_t*>(m_Allocator->Allocate(static_cast<size_t>(size)));
			Size = size;
		}

		void Release()
		{
			if (!Data)
			{
				Size = 0;
				return;
			}
			m_Allocator->Free(Data);
			Data = nullptr;
			Size = 0;
		}

		template<typename T>
		T* As()
		{
			return (T*)Data;
		}

		template<typename T>
		const T* As() const
		{
			return (const T*)Data;
		}

		operator bool() const
		{
			return (bool)Data;
		}
	private:
		IAllocator* m_Allocator = MemorySystem::GetAllocator();
	};

	class ScopedBuffer
	{
	public:
		explicit ScopedBuffer(Buffer&& buffer)
			: m_Buffer(std::move(buffer))
		{
		}

		explicit ScopedBuffer(uint64_t size)
			: m_Buffer(size)
		{
		}

		~ScopedBuffer() = default;

		uint8_t* Data() { return m_Buffer.Data; }
		uint64_t Size() { return m_Buffer.Size; }

		template<typename T>
		T* As()
		{
			return m_Buffer.As<T>();
		}

		operator bool() const { return m_Buffer; }
	private:
		Buffer m_Buffer;
	};


}
