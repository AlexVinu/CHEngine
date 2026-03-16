#include <chepch.h>
#include "ImGuiLayerOGL.h"

#include <Log/Log.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace CHModules {

    ImGuiLayerOGL::ImGuiLayerOGL(CHEngine::IWindow* window)
    {
        if (!window) {
            CHE_CORE_ERROR("ImGuiLayerOGL: window is null!");
            return;
        }

        // static_cast только здесь, внутри модуля — снаружи никто не знает о GLFW
        GLFWwindow* glfwWin = static_cast<GLFWwindow*>(window->GetNativeWindow());
        if (!glfwWin) {
            CHE_CORE_ERROR("ImGuiLayerOGL: GetNativeWindow() вернул null!");
            return;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

#ifdef CHE_PLATFORM_APPLE
        ImGui_ImplGlfw_InitForOpenGL(glfwWin, true);
        ImGui_ImplOpenGL3_Init("#version 410");
#else
        ImGui_ImplGlfw_InitForOpenGL(glfwWin, true);
        ImGui_ImplOpenGL3_Init("#version 460");
#endif
    }

    ImGuiLayerOGL::~ImGuiLayerOGL()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayerOGL::Begin()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayerOGL::End()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

}
