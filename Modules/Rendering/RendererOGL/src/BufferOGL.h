#pragma once

#include "Render/Core/RenderTypes.h"

#include <glad/glad.h>
#include <span>

namespace CHModules {

	class BufferOGL {
	public:
		BufferOGL(uint64_t size,
			CHEngine::BufferUsage usage,
			CHEngine::MemoryType memory,
			std::span<const std::byte> initialData,
			const String& debugName);

		~BufferOGL();

		// ��������� �����������
		BufferOGL(const BufferOGL&) = delete;
		BufferOGL& operator=(const BufferOGL&) = delete;

		// ��������� �����������
		BufferOGL(BufferOGL&& other) noexcept;
		BufferOGL& operator=(BufferOGL&& other) noexcept;

		// �������� ��������
		void Bind() const;
		void Unbind() const;

		// ���������� ������
		void UpdateData(const void* data, uint64_t size, uint64_t offset = 0);
		void UpdateData(std::span<const std::byte> data, uint64_t offset = 0);

		// ������� ��� ������� �������
		void* Map(uint64_t offset, uint64_t length, GLbitfield access);
		void Unmap();

		// �������
		GLuint GetID() const { return m_RendererID; }
		uint64_t GetSize() const { return m_Size; }
		CHEngine::BufferUsage GetUsage() const { return m_Usage; }
		CHEngine::MemoryType GetMemoryType() const { return m_Memory; }
		GLenum GetTarget() const { return m_Target; }

		// ���������� � ������������ binding point (��� Uniform/Storage �������)
		void BindBase(GLuint index) const;
		void BindRange(GLuint index, uint64_t offset, uint64_t size) const;

	private:
		GLenum GetGLTarget(CHEngine::BufferUsage usage) const;
		GLenum GetGLUsage(CHEngine::BufferUsage bufferUsage, CHEngine::MemoryType memory) const;

		GLuint m_RendererID = 0;
		GLenum m_Target = GL_ARRAY_BUFFER;
		uint64_t m_Size = 0;
		CHEngine::BufferUsage m_Usage;
		CHEngine::MemoryType m_Memory;
		bool m_IsMapped = false;
	};
}
