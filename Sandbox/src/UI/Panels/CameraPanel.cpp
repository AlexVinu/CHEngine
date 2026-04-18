#include "CameraPanel.h"

#include "EditorCamera.h"
#include "SceneSession.h"
#include "SetTransformCommand.h"

#include "UIThemeActive.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Sandbox {

void CameraPanel::Draw(EditorUiHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Camera", pos, size, 0, reset_layout);
    SceneSession& activeSession = host.GetActiveSceneSession();
    CHEngine::EditorCamera& viewportCamera = *activeSession.ViewportCamera;

    UIActive::SectionHeader("VIEW");

    if (UIActive::PrimaryButton("Perspective", ImVec2(-1.0f, 0.0f)))
    {
        host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
            [&host] {
                host.SetViewPreset(-90.0f, -15.0f);
                host.SetViewportFov(45.0f);
            },
            [] {}, false));
    }

    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    float hw = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;

    struct ViewPreset {
        const char* name;
        float yaw;
        float pitch;
    };
    ViewPreset presets[] = {
        { "Front", -90.0f, 0.0f },
        { "Back", 90.0f, 0.0f },
        { "Top", -90.0f, -89.9f },
        { "Bottom", -90.0f, 89.9f },
        { "Right", 180.0f, 0.0f },
        { "Left", 0.0f, 0.0f },
    };
    for (int i = 0; i < 6; i++)
    {
        if (i % 2 != 0)
            ImGui::SameLine();
        if (ImGui::Button(presets[i].name, ImVec2(hw, 0)))
        {
            const float yaw = presets[i].yaw;
            const float pitch = presets[i].pitch;
            host.GetCommandStack().Push(CHEngine::MakeScope<CallbackCommand>(
                [&host, yaw, pitch] { host.SetViewPreset(yaw, pitch); }, [] {}, false));
        }
    }
    ImGui::PopStyleVar();

    UIActive::SectionHeader("ORBIT");

    bool orb = false;
    const float lw = 64.0f;

    auto sliderRow = [&](const char* label, const char* id, float* val, float mn, float mx, const char* fmt) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(lw);
        ImGui::SetNextItemWidth(-1.0f);
        return ImGui::SliderFloat(id, val, mn, mx, fmt);
    };
    auto dragRow = [&](const char* label, const char* id, float* val, float spd, float mn, float mx) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(lw);
        ImGui::SetNextItemWidth(-1.0f);
        return ImGui::DragFloat(id, val, spd, mn, mx, "%.2f");
    };

    float yaw = glm::degrees(viewportCamera.GetYaw());
    float pitch = glm::degrees(viewportCamera.GetPitch());
    float fov = viewportCamera.GetFOV();

    if (sliderRow("Yaw", "##yaw", &yaw, -180.0f, 180.0f, "%.1f°"))
    {
        viewportCamera.SetYaw(glm::radians(yaw));
        orb = true;
    }
    if (sliderRow("Pitch", "##pitch", &pitch, -89.0f, 89.0f, "%.1f°"))
    {
        viewportCamera.SetPitch(glm::radians(pitch));
        orb = true;
    }
    if (sliderRow("FOV", "##fov", &fov, 10.0f, 120.0f, "%.1f°"))
        viewportCamera.SetFOV(fov);
    float orbitDist = host.GetEditorCamera().GetOrbitDist();
    if (dragRow("Dist", "##dist", &orbitDist, 0.1f, 0.3f, 500.0f))
    {
        host.GetEditorCamera().SetOrbitDist(orbitDist);
        orb = true;
    }

    if (orb)
        host.ApplyOrbit();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
    ImGui::TextUnformatted("Target");
    ImGui::PopStyleColor();
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(-1.0f);
    glm::vec3 orbitTarget = host.GetEditorCamera().GetOrbitTarget();
    if (ImGui::DragFloat3("##target", glm::value_ptr(orbitTarget), 0.05f))
    {
        host.GetEditorCamera().SetOrbitTarget(orbitTarget);
        host.ApplyOrbit();
    }

    glm::vec3 cpos = viewportCamera.GetPosition();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.357f, 0.357f, 0.376f, 1.0f));
    ImGui::Text("Pos  %.2f  %.2f  %.2f", cpos.x, cpos.y, cpos.z);
    ImGui::PopStyleColor();

    UIActive::SectionHeader("CONTROLS");
    bool followObject = host.GetEditorCamera().GetFollowObject();
    if (UIActive::Toggle("Follow Selected", &followObject))
        host.GetEditorCamera().SetFollowObject(followObject);

    ImGui::Spacing();
    if (UIActive::DestructiveButton("Reset Camera", ImVec2(-1.0f, 0.0f)))
    {
        host.GetCommandStack().Push(
            CHEngine::MakeScope<CallbackCommand>([&host] { host.ResetViewportCamera(); }, [] {}, false));
    }

    UIActive::EndPanel();
}

} // namespace Sandbox
