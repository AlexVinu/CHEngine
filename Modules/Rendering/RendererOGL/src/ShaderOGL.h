#pragma once

#include <CheStl/String.h>

#include "Render/UniformBlocks.h"

#include <glad/glad.h>
#include <array>

namespace CHModules
{
	class ShaderOGL
	{
	public:
		ShaderOGL(const CHEngine::String& slangSource,
		          const CHEngine::String& vertEntry,
		          const CHEngine::String& fragEntry,
		          const CHEngine::String& sourcePath = CHEngine::String());
		 ~ShaderOGL();

		ShaderOGL(const ShaderOGL&) = delete;
		ShaderOGL& operator=(const ShaderOGL&) = delete;

		ShaderOGL(ShaderOGL&& other) = delete;
		ShaderOGL& operator=(ShaderOGL&& other) = delete;

		 void Bind() const;
		 void Unbind() const;

		 bool Reload(const CHEngine::String& slangSource,
		                    const CHEngine::String& vertEntry  = CHEngine::String("vertMain"),
		                    const CHEngine::String& fragEntry  = CHEngine::String("fragMain"),
		                    const CHEngine::String& sourcePath = CHEngine::String()) ;

		 void SetUniformBlock(CHEngine::EUniformBlock block, const void* data, uint32_t size);

		// --- Sampler binding ---
		 void SetInt(const CHEngine::String& name, int value) ;

	private:
		GLuint m_ProgramID;
		// Compile a .slang source through SlangBackend (→ GLSL) and link a GL program.
		// Returns the program ID on success, 0 on failure.
		static GLuint CompileSlangProgram(const CHEngine::String& slangSource,
		                                  const CHEngine::String& vertEntry,
		                                  const CHEngine::String& fragEntry,
		                                  const CHEngine::String& sourcePath);

		// Auto-assign sampler uniforms to sequential texture units (0,1,2,...)
		// in declaration order, regardless of Slang's emitted binding indices.
		void BindSamplerUnits();
		
		void BindUBOBlocks();

		// OpenGL Uniform Buffer Objects (по одному на каждый binding point)
		static constexpr uint32_t UBO_COUNT = static_cast<uint32_t>(CHEngine::EUniformBlock::COUNT);
		std::array<GLuint, UBO_COUNT> m_UBOs = {0};
	};
}
