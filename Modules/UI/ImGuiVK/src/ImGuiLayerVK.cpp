#include "chepch.h"
#include "ImGuiLayerVK.h"

#include <Log/Log.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace CHModules {

    // Vulkan ImGui требует множество параметров от VulkanContext.
    // Скелетная реализация — инициализирует только GLFW backend.
    // Полная интеграция требует передачи VkDevice, VkRenderPass и т.д.

    ImGuiLayerVK::ImGuiLayerVK(CHEngine::IWindow* window)
    {
        if (!window) {
            CHE_CORE_ERROR("ImGuiLayerVK: window is null!");
            return;
        }

        GLFWwindow* glfwWin = static_cast<GLFWwindow*>(window->GetNativeWindow());
        if (!glfwWin) {
            CHE_CORE_ERROR("ImGuiLayerVK: GetNativeWindow() вернул null!");
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        // Инициализация GLFW backend для Vulkan
        ImGui_ImplGlfw_InitForVulkan(glfwWin, true);

        // ВАЖНО: ImGui_ImplVulkan_Init требует VkDevice, VkRenderPass, VkDescriptorPool и т.д.
        // Для полной инициализации нужен доступ к VulkanContext.
        // Сейчас это скелетная реализация — рендеринг ImGui не будет виден,
        // но GLFW-ввод (клавиатура, мышь) будет работать.
        CHE_CORE_WARN("ImGuiLayerVK: Vulkan ImGui backend — скелетная реализация. "
                      "ImGui рендеринг будет недоступен до полной интеграции с VulkanContext.");

        m_Initialized = true;
    }

    ImGuiLayerVK::~ImGuiLayerVK()
    {
        if (m_Initialized) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }
    }

    void ImGuiLayerVK::Begin()
    {
        if (!m_Initialized) return;
        // ImGui_ImplVulkan_NewFrame() — вызовется когда будет полная интеграция
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayerVK::End()
    {
        if (!m_Initialized) return;
        ImGui::Render();
        // ImGui_ImplVulkan_RenderDrawData() — вызовется когда будет полная интеграция
    }

}
