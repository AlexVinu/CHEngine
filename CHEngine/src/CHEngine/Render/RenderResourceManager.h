#pragma once

#include <Core.h>
#include <Memory/Handle.h>
#include <Memory/HandlePool.h>
#include <Containers/String.h>

#include "Render/IRenderFactory.h"

#include <memory>

namespace CHEngine {

	struct ShaderTag {};
	struct VertexArrayTag {};
	struct RenderAPITag {};

	using ShaderHandle      = Handle<ShaderTag>;
	using VertexArrayHandle = Handle<VertexArrayTag>;
	using RenderAPIHandle   = Handle<RenderAPITag>;

	class CHENGINE_API RenderResourceManager
	{
	public:
		RenderResourceManager() = default;

		void Init(IRenderFactory* factory);

		ShaderHandle      CreateShader(const String& vertexSrc, const String& fragmentSrc);
		ShaderHandle      CreateShaderFromFile(const String& vertexPath, const String& fragmentPath);
		VertexArrayHandle CreateVertexArray();
		RenderAPIHandle   CreateRenderAPI();

		std::shared_ptr<IVertexBuffer> CreateVertexBuffer(float* vertices, uint32_t size);
		std::shared_ptr<IIndexBuffer>  CreateIndexBuffer(uint32_t* indices, uint32_t count);

		IShader*      Get(ShaderHandle h) const;
		IVertexArray* Get(VertexArrayHandle h) const;
		RendererAPI*  Get(RenderAPIHandle h) const;

		void DestroyShader(ShaderHandle h);
		void DestroyVertexArray(VertexArrayHandle h);
		void DestroyRenderAPI(RenderAPIHandle h);

		void Shutdown();

	private:
		static String ReadTextFile(const String& path);

		IRenderFactory* m_Factory = nullptr;

		HandlePool<IShader,      ShaderTag>      m_Shaders;
		HandlePool<IVertexArray, VertexArrayTag>  m_VertexArrays;
		HandlePool<RendererAPI,  RenderAPITag>    m_RenderApis;
	};

}
