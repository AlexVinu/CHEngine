#include <cstdint>
#include "ImGuiLayerMTL.h"

#include <Log/Log.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_metal.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace CHModules {

    ImGuiLayerMTL::ImGuiLayerMTL(CHEngine::IWindow* window)
    {
        if (!window) {
            CHE_CORE_ERROR("ImGuiLayerMTL: window is null!");
            return;
        }

        GLFWwindow* glfwWin = static_cast<GLFWwindow*>(window->GetNativeWindow());
        if (!glfwWin) {
            CHE_CORE_ERROR("ImGuiLayerMTL: GetNativeWindow() returned null!");
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        // GLFW backend (для ввода — не OpenGL)
        ImGui_ImplGlfw_InitForOther(glfwWin, true);

        m_Initialized = true;
        CHE_CORE_INFO("ImGuiLayerMTL: GLFW backend initialized, waiting for Metal context...");
    }

    ImGuiLayerMTL::~ImGuiLayerMTL()
    {
        if (m_MetalBackendReady)
            ImGui_ImplMetal_Shutdown();
        if (m_Initialized) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
    }

    void ImGuiLayerMTL::SetRenderContext(const CHEngine::RenderContextInfo& ctx)
    {
        // Инициализация Metal-бэкенда при первом получении контекста
        if (!m_MetalBackendReady && ctx.Device) {
            id<MTLDevice> device = (id<MTLDevice>)ctx.Device;
            ImGui_ImplMetal_Init(device);
            m_MetalBackendReady = true;
            CHE_CORE_INFO("ImGuiLayerMTL: Metal backend initialized ({})",
                          [[device name] UTF8String]);
        }

        m_CommandBuffer  = ctx.CommandBuffer;
        m_RenderEncoder  = ctx.RenderEncoder;
        m_RenderPassDesc = ctx.RenderPassDescriptor;
    }

    void ImGuiLayerMTL::Begin()
    {
        if (!m_Initialized) return;

        ImGui_ImplGlfw_NewFrame();

        if (m_MetalBackendReady && m_RenderPassDesc) {
            MTLRenderPassDescriptor* rpDesc = (MTLRenderPassDescriptor*)m_RenderPassDesc;
            ImGui_ImplMetal_NewFrame(rpDesc);
        }

        ImGui::NewFrame();
    }

    void ImGuiLayerMTL::End()
    {
        if (!m_Initialized) return;

        ImGui::Render();

        if (m_MetalBackendReady && m_CommandBuffer && m_RenderEncoder) {
            id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)m_CommandBuffer;
            id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)m_RenderEncoder;
            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), cmdBuf, encoder);
        }
    }

}
