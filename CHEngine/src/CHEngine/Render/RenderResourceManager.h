#pragma once

#include <Core.h>
#include <Memory/Handle.h>
#include <Memory/HandlePool.h>
#include <Containers/String.h>

#include "Render/IRenderFactory.h"

#include <memory>
#include <vector>

namespace CHEngine {

	struct ShaderTag {};
	struct VertexArrayTag {};
	struct RenderAPITag {};
	struct TextureTag {};

	using ShaderHandle      = Handle<ShaderTag>;
	using VertexArrayHandle = Handle<VertexArrayTag>;
	using RenderAPIHandle   = Handle<RenderAPITag>;
	using TextureHandle     = Handle<TextureTag>;

	// Metadata stored for every named shader (enables hot-reload and manager UI).
	struct ShaderEntry
	{
		String      name;
		String      vertPath;
		String      fragPath;
		ShaderHandle handle;
		bool        valid = false;
	};

	class CHENGINE_API RenderResourceManager
	{
	public:
		RenderResourceManager() = default;

		void Init(IRenderFactory* factory);

		// Unnamed — creates a shader without registering it in the shader list.
		ShaderHandle CreateShader(const String& vertexSrc, const String& fragmentSrc);

		// Creates a shader from files and registers it for hot-reload / manager UI.
		ShaderHandle CreateShaderFromFile(const String& name,
		                                  const String& vertexPath,
		                                  const String& fragmentPath);

		VertexArrayHandle CreateVertexArray();
		RenderAPIHandle   CreateRenderAPI();

		// Upload raw pixel data as a GPU texture.
		// channels: 1=R, 2=RG, 3=RGB, 4=RGBA.
		TextureHandle CreateTexture(const uint8_t* data, uint32_t width,
		                            uint32_t height, uint32_t channels);

		std::shared_ptr<IVertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size);
		std::shared_ptr<IIndexBuffer>  CreateIndexBuffer(uint32_t* indices, uint32_t count);

		IShader*      Get(ShaderHandle h) const;
		IVertexArray* Get(VertexArrayHandle h) const;
		RendererAPI*  Get(RenderAPIHandle h) const;
		ITexture*     Get(TextureHandle h) const;

		// Re-reads shader files from disk and recompiles in-place.
		// Returns true on success; the old GPU program remains on failure.
		bool ReloadShader(ShaderHandle h);

		const std::vector<ShaderEntry>& GetShaderEntries() const;

		void DestroyShader(ShaderHandle h);
		void DestroyVertexArray(VertexArrayHandle h);
		void DestroyRenderAPI(RenderAPIHandle h);
		void DestroyTexture(TextureHandle h);

		void Shutdown();

	private:
		static String ReadTextFile(const String& path);

		IRenderFactory* m_Factory = nullptr;

		HandlePool<IShader,      ShaderTag>      m_Shaders;
		HandlePool<IVertexArray, VertexArrayTag>  m_VertexArrays;
		HandlePool<RendererAPI,  RenderAPITag>    m_RenderApis;
		HandlePool<ITexture,     TextureTag>      m_Textures;

		std::vector<ShaderEntry> m_ShaderEntries;
	};

}
