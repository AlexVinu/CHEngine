#pragma once

#include <CheStl/String.h>

#include "Render/UniformBlocks.h"
#include <SlangBackend/SlangBackend.h>

#include <glad/glad.h>
#include <array>

namespace CHModules
{
	class ShaderOGL
	{
	public:
		// slang: reference to the factory-owned SlangBackend (must outlive this shader)
		ShaderOGL(const String& slangSource,
		          const String& vertEntry,
		          const String& fragEntry,
		          const String& sourcePath,
		          SlangBackend& slang);
		 ~ShaderOGL();

		ShaderOGL(const ShaderOGL&) = delete;
		ShaderOGL& operator=(const ShaderOGL&) = delete;

		ShaderOGL(ShaderOGL&& other) = delete;
		ShaderOGL& operator=(ShaderOGL&& other) = delete;

		 void Bind() const;
		 void Unbind() const;

		 bool Reload(const String& slangSource,
		                    const String& vertEntry  = String("vertMain"),
		                    const String& fragEntry  = String("fragMain"),
		                    const String& sourcePath = String()) ;

		 void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size);

		// --- Sampler binding ---
		 void SetInt(const String& name, int value) ;

	private:
		GLuint        m_ProgramID;
		SlangBackend* m_Slang = nullptr; // owned by the factory; always valid during shader lifetime

		// Compile a .slang source through SlangBackend (→ GLSL) and link a GL program.
		// Returns the program ID on success, 0 on failure.
		GLuint CompileSlangProgram(const String& slangSource,
		                           const String& vertEntry,
		                           const String& fragEntry,
		                           const String& sourcePath);

		// Auto-assign sampler uniforms to sequential texture units (0,1,2,...)
		// in declaration order, regardless of Slang's emitted binding indices.
		void BindSamplerUnits();
		
		void BindUBOBlocks();

		// OpenGL Uniform Buffer Objects (по одному на каждый binding point)
		static constexpr uint32_t UBO_COUNT = static_cast<uint32_t>(CHEngine::EUniformBlock::COUNT);
		std::array<GLuint, UBO_COUNT> m_UBOs = {0};
	};
}
