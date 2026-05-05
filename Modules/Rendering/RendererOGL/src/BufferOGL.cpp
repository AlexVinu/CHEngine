#pragma once

#include "BufferOGL.h"

#include <cassert>
#include <cstring>

#include <glad/glad.h>

namespace CHModules {

	// ================ ��������������� ������� ================

	GLenum BufferOGL::GetGLTarget(CHEngine::BufferUsage usage) const {
		switch (usage) {
		case CHEngine::BufferUsage::Vertex:
			return GL_ARRAY_BUFFER;
		case CHEngine::BufferUsage::Index:
			return GL_ELEMENT_ARRAY_BUFFER;
		case CHEngine::BufferUsage::Uniform:
			return GL_UNIFORM_BUFFER;
		case CHEngine::BufferUsage::Storage:
			return GL_SHADER_STORAGE_BUFFER;
		case CHEngine::BufferUsage::Indirect:
			return GL_DRAW_INDIRECT_BUFFER;
			// ����� �������� ������ ����
		default:
			return GL_ARRAY_BUFFER;
		}
	}

	GLenum BufferOGL::GetGLUsage(CHEngine::BufferUsage bufferUsage, CHEngine::MemoryType memory) const {
		switch (memory) {
		case CHEngine::MemoryType::GpuOnly:
			// ������ ����� ��������, ������ ��� GPU
			return GL_STATIC_DRAW;

		case CHEngine::MemoryType::CpuToGpu:
			// ������ ���������� � CPU
			if (bufferUsage == CHEngine::BufferUsage::Uniform ||
				bufferUsage == CHEngine::BufferUsage::Storage) {
				return GL_DYNAMIC_DRAW;
			}
			return GL_STREAM_DRAW;

		case CHEngine::MemoryType::GpuToCpu:
			// ������ � GPU ������� �� CPU
			return GL_DYNAMIC_READ;

		default:
			return GL_STATIC_DRAW;
		}
	}

	// ================ ����������� ================

	BufferOGL::BufferOGL(uint64_t size,
		CHEngine::BufferUsage usage,
		CHEngine::MemoryType memory,
		std::span<const std::byte> initialData,
		const CHEngine::String& debugName)
		: m_Size(size)
		, m_Usage(usage)
		, m_Memory(memory)
	{
		// ���������� ���� ������
		m_Target = GetGLTarget(usage);

		// ���������� ��������� �������������
		GLenum glUsage = GetGLUsage(usage, memory);

		// ���������� �����
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(m_Target, m_RendererID);

		// �������� ������ � ��������� ��������� ������
		const void* dataPtr = initialData.empty() ? nullptr : initialData.data();
		uint64_t dataSize = initialData.empty() ? size : initialData.size();

		// ���� ���� ��������� ������, �� ������ ����� ���� ������ ������� ������
		if (dataPtr && dataSize <= size) {
			glBufferData(m_Target, static_cast<GLsizeiptr>(size), nullptr, glUsage);
			glBufferSubData(m_Target, 0, static_cast<GLsizeiptr>(dataSize), dataPtr);
		}
		else {
			glBufferData(m_Target, static_cast<GLsizeiptr>(size), dataPtr, glUsage);
		}

		// ������������� ���������� ���
		if (!debugName.empty() && glObjectLabel) {
			glObjectLabel(GL_BUFFER, m_RendererID,
				static_cast<GLsizei>(debugName.length()),
				debugName.c_str());
		}

		// ��������� �� ������
		assert(glGetError() == GL_NO_ERROR);

		glBindBuffer(m_Target, 0);
	}

	// ================ ���������� ================

	BufferOGL::~BufferOGL() {
		if (m_RendererID) {
			glDeleteBuffers(1, &m_RendererID);
			m_RendererID = 0;
		}
	}

	// ================ ����������� ================

	BufferOGL::BufferOGL(BufferOGL&& other) noexcept
		: m_RendererID(other.m_RendererID)
		, m_Target(other.m_Target)
		, m_Size(other.m_Size)
		, m_Usage(other.m_Usage)
		, m_Memory(other.m_Memory)
		, m_IsMapped(other.m_IsMapped)
	{
		other.m_RendererID = 0;
		other.m_Size = 0;
		other.m_IsMapped = false;
	}

	BufferOGL& BufferOGL::operator=(BufferOGL&& other) noexcept {
		if (this != &other) {
			// ������� ������� �����
			if (m_RendererID) {
				glDeleteBuffers(1, &m_RendererID);
			}

			// ���������� ������
			m_RendererID = other.m_RendererID;
			m_Target = other.m_Target;
			m_Size = other.m_Size;
			m_Usage = other.m_Usage;
			m_Memory = other.m_Memory;
			m_IsMapped = other.m_IsMapped;

			other.m_RendererID = 0;
			other.m_Size = 0;
			other.m_IsMapped = false;
		}
		return *this;
	}

	// ================ �������� �������� ================

	void BufferOGL::Bind() const {
		glBindBuffer(m_Target, m_RendererID);
	}

	void BufferOGL::Unbind() const {
		glBindBuffer(m_Target, 0);
	}

	void BufferOGL::UpdateData(const void* data, uint64_t size, uint64_t offset) {
		assert(offset + size <= m_Size);
		assert(!m_IsMapped && "Cannot update mapped buffer");

		glBindBuffer(m_Target, m_RendererID);
		glBufferSubData(m_Target,
			static_cast<GLintptr>(offset),
			static_cast<GLsizeiptr>(size),
			data);
		glBindBuffer(m_Target, 0);
	}

	void BufferOGL::UpdateData(std::span<const std::byte> data, uint64_t offset) {
		UpdateData(data.data(), data.size(), offset);
	}

	void* BufferOGL::Map(uint64_t offset, uint64_t length, GLbitfield access) {
		assert(!m_IsMapped && "Buffer is already mapped");
		assert(offset + length <= m_Size);

		glBindBuffer(m_Target, m_RendererID);
		void* ptr = glMapBufferRange(m_Target,
			static_cast<GLintptr>(offset),
			static_cast<GLsizeiptr>(length),
			access);

		if (ptr) {
			m_IsMapped = true;
		}

		return ptr;
	}

	void BufferOGL::Unmap() {
		assert(m_IsMapped && "Buffer is not mapped");

		glBindBuffer(m_Target, m_RendererID);
		glUnmapBuffer(m_Target);
		m_IsMapped = false;
		glBindBuffer(m_Target, 0);
	}

	void BufferOGL::BindBase(GLuint index) const {
		assert(m_Target == GL_UNIFORM_BUFFER ||
			m_Target == GL_SHADER_STORAGE_BUFFER ||
			m_Target == GL_ATOMIC_COUNTER_BUFFER);

		glBindBufferBase(m_Target, index, m_RendererID);
	}

	void BufferOGL::BindRange(GLuint index, uint64_t offset, uint64_t size) const {
		assert(m_Target == GL_UNIFORM_BUFFER ||
			m_Target == GL_SHADER_STORAGE_BUFFER ||
			m_Target == GL_ATOMIC_COUNTER_BUFFER);
		assert(offset + size <= m_Size);

		glBindBufferRange(m_Target, index, m_RendererID,
			static_cast<GLintptr>(offset),
			static_cast<GLsizeiptr>(size));
	}
}