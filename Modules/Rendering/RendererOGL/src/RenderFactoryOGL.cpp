#include "RenderFactoryOGL.h"

#include "BufferOGL.h"
#include "VertexArrayOGL.h"
#include "ShaderOGL.h"
#include "RendererOGL.h"
#include "RenderApiOGL.h"

namespace CHModules
{
	CHEngine::IVertexBuffer* RenderFactoryOGL::CreateVertexBuffer(float* verticies, uint32_t size)
	{
		return new CHModules::VertexBufferOGL(verticies, size);
	}
	CHEngine::IIndexBuffer* RenderFactoryOGL::CreateIndexBuffer(uint32_t* indices, uint32_t count)
	{
		return new CHModules::IndexBufferOGL(indices, count);
	}
	CHEngine::IVertexArray* RenderFactoryOGL::CreateVertexArray()
	{
		return new CHModules::VertexArrayOGL();
	}
	CHEngine::IShader* RenderFactoryOGL::CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
	{
		return new CHModules::ShaderOGL(vertexSrc, fragmentSrc);
	}
	CHEngine::RendererAPI* RenderFactoryOGL::CreateRenderAPI()
	{
		return new CHModules::RendererApiOGL();
	}
	CHEngine::IRenderer* RenderFactoryOGL::CreateRenderer(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn)
	{
		return new CHModules::RendererOGL(width, height, title, errorCallbackFn);
	}
	CHEngine::ModuleType RenderFactoryOGL::GetType() const
	{
		return CHEngine::ModuleType::Render;
	}
	void RenderFactoryOGL::Delete(CHEngine::IVertexBuffer* ptr){delete ptr;}
	void RenderFactoryOGL::Delete(CHEngine::IIndexBuffer* ptr) { delete ptr; }
	void RenderFactoryOGL::Delete(CHEngine::IVertexArray* ptr) { delete ptr; }
	void RenderFactoryOGL::Delete(CHEngine::IShader* ptr) { delete ptr; }
	void RenderFactoryOGL::Delete(CHEngine::RendererAPI* ptr) { delete ptr; }
	void RenderFactoryOGL::Delete(CHEngine::IRenderer* ptr) { delete ptr; }
}

extern "C"
{
	CHE_MODULE_API CHEngine::IModuleFactory* CreateFactory()
	{
		return new CHModules::RenderFactoryOGL();
	}

	CHE_MODULE_API void DestroyFactory(CHEngine::IModuleFactory* factory)
	{
		delete factory;
	}
}