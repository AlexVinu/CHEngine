#include "RenderFactoryOGL.h"

#include "ImGuiLayerOGL.h"
#include "BufferOGL.h"
#include "VertexArrayOGL.h"
#include "ShaderOGL.h"
#include "RendererOGL.h"
#include "RenderApiOGL.h"

namespace CHModules
{
	CHEngine::IVertexBuffer* RenderFactoryOGL::CreateVertexBuffer(float* verticies, uint32_t size)
	{
		CHE_MODULE_INFO("VertexBufferOGL CREATED");
		return new CHModules::VertexBufferOGL(verticies, size);
	}
	CHEngine::IIndexBuffer* RenderFactoryOGL::CreateIndexBuffer(uint32_t* indices, uint32_t count)
	{
		CHE_MODULE_INFO("IndexBufferOGL CREATED");
		return new CHModules::IndexBufferOGL(indices, count);
	}
	CHEngine::IVertexArray* RenderFactoryOGL::CreateVertexArray()
	{
		CHE_MODULE_INFO("VertexArrayOGL CREATED");
		return new CHModules::VertexArrayOGL();
	}
	CHEngine::IShader* RenderFactoryOGL::CreateShader(const CHEngine::String& vertexSrc, const CHEngine::String& fragmentSrc)
	{
		CHE_MODULE_INFO("ShaderOGL CREATED");
		return new CHModules::ShaderOGL(vertexSrc, fragmentSrc);
	}
	CHEngine::RendererAPI* RenderFactoryOGL::CreateRenderAPI()
	{
		CHE_MODULE_INFO("RendererApiOGL CREATED");
		return new CHModules::RendererApiOGL();
	}
	CHEngine::IRenderer* RenderFactoryOGL::CreateRenderer(const unsigned int width, const unsigned int height, const char* title, CHEngine::ErrorCallbackFn errorCallbackFn)
	{
		CHE_MODULE_INFO("RendererOGL CREATED");
		return new CHModules::RendererOGL(width, height, title, errorCallbackFn);
	}
	CHEngine::ModuleType RenderFactoryOGL::GetType() const
	{
		return CHEngine::ModuleType::Render;
	}
	void RenderFactoryOGL::Delete(CHEngine::IVertexBuffer* ptr)
	{
		CHE_MODULE_INFO("VertexBufferOGL DELETED");
		delete ptr;
	}
	void RenderFactoryOGL::Delete(CHEngine::IIndexBuffer* ptr) 
	{
		CHE_MODULE_INFO("IndexBufferOGL DELETED");
		delete ptr; 
	}
	void RenderFactoryOGL::Delete(CHEngine::IVertexArray* ptr) 
	{ 
		CHE_MODULE_INFO("VertexArrayOGL DELETED");
		delete ptr; 
	}
	void RenderFactoryOGL::Delete(CHEngine::IShader* ptr) 
	{
		CHE_MODULE_INFO("ShaderOGL DELETED");
		delete ptr; 
	}
	void RenderFactoryOGL::Delete(CHEngine::RendererAPI* ptr) 
	{
		CHE_MODULE_INFO("RendererApiOGL DELETED");
		delete ptr; 
	}
	void RenderFactoryOGL::Delete(CHEngine::IRenderer* ptr) 
	{
		CHE_MODULE_INFO("RendererOGL DELETED");
		delete ptr; 
	}
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
// ImGui layer methods (appended)
namespace CHModules {
    CHEngine::IImGuiLayer* RenderFactoryOGL::CreateImGuiLayer(void* nativeWindow)
    {
        CHE_MODULE_INFO("ImGuiLayerOGL CREATED");
        return new CHModules::ImGuiLayerOGL(nativeWindow);
    }
    void RenderFactoryOGL::Delete(CHEngine::IImGuiLayer* ptr)
    {
        CHE_MODULE_INFO("ImGuiLayerOGL DELETED");
        delete ptr;
    }
}
