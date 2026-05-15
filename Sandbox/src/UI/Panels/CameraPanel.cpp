#include "CameraPanel.h"
#include "SceneHierarchyPanel.h"

#include "EditorCameraController.h"
#include "EditorWorldContext.h"
#include "SceneSession.h"
#include "SetTransformCommand.h"

#include "UIThemeActive.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Sandbox {

void CameraPanel::Draw(SceneViewLayerHost& host, ImVec2 pos, ImVec2 size, bool reset_layout)
{
    UIActive::BeginPanel("Inspector", pos, size, 0, reset_layout);

    // ── Tab bar: Scene (default) | Camera ────────────────────────────────────
    if (ImGui::BeginTabBar("##inspector_tabs"))
    {
        // ── Scene tab (первая и активная по умолчанию) ────────────────────────
        if (ImGui::BeginTabItem("Scene"))
        {
            m_Hierarchy.DrawContent(host);
            ImGui::EndTabItem();
        }

        // ── Camera tab ────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Camera"))
        {
            Ref<EditorWorldContext> activeSession = host.GetActiveSceneSession();
            auto* viewport_camera = activeSession->ViewportCamera.get();
            if (viewport_camera)
            {
                Sandbox::EditorCameraState& cameraState = activeSession->EditorCameraState;

                UIActive::SectionHeader("VIEW");

                if (UIActive::PrimaryButton("Perspective", ImVec2(-1.0f, 0.0f)))
                {
                    host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                        [&host] {
                            host.SetViewPreset(-90.0f, -15.0f);
                            host.SetViewportFov(45.0f);
                        }, [] {}, false));
                }

                ImGui::Spacing();
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                float hw = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;

                struct ViewPreset { const char* name; float yaw; float pitch; };
                ViewPreset presets[] = {
                    { "Front", -90.0f, 0.0f },  { "Back",   90.0f,  0.0f   },
                    { "Top",   -90.0f,-89.9f },  { "Bottom",-90.0f, 89.9f   },
                    { "Right", 180.0f,  0.0f },  { "Left",    0.0f,  0.0f   },
                };
                for (int i = 0; i < 6; i++)
                {
                    if (i % 2 != 0) ImGui::SameLine();
                    if (ImGui::Button(presets[i].name, ImVec2(hw, 0)))
                    {
                        const float y = presets[i].yaw, p = presets[i].pitch;
                        host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                            [&host, y, p] { host.SetViewPreset(y, p); }, [] {}, false));
                    }
                }
                ImGui::PopStyleVar();

                UIActive::SectionHeader("ORBIT");

                bool orb = false;
                const float lw = 64.0f;
                auto sliderRow = [&](const char* label, const char* id,
                                     float* val, float mn, float mx, const char* fmt) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted(label);
                    ImGui::PopStyleColor();
                    ImGui::SameLine(lw);
                    ImGui::SetNextItemWidth(-1.0f);
                    return ImGui::SliderFloat(id, val, mn, mx, fmt);
                };
                auto dragRow = [&](const char* label, const char* id,
                                   float* val, float spd, float mn, float mx) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                    ImGui::TextUnformatted(label);
                    ImGui::PopStyleColor();
                    ImGui::SameLine(lw);
                    ImGui::SetNextItemWidth(-1.0f);
                    return ImGui::DragFloat(id, val, spd, mn, mx, "%.2f");
                };

                float yaw   = glm::degrees(viewport_camera->GetYaw());
                float pitch = glm::degrees(viewport_camera->GetPitch());
                float fov   = viewport_camera->GetFOV();

                if (sliderRow("Yaw",   "##yaw",   &yaw,   -180.0f, 180.0f, "%.1f°")) { viewport_camera->SetYaw(glm::radians(yaw));     orb = true; }
                if (sliderRow("Pitch", "##pitch", &pitch, -89.0f,   89.0f, "%.1f°")) { viewport_camera->SetPitch(glm::radians(pitch));  orb = true; }
                if (sliderRow("FOV",   "##fov",   &fov,    10.0f,  120.0f, "%.1f°"))  viewport_camera->SetFOV(fov);

                float orbitDist = cameraState.OrbitDist;
                if (dragRow("Dist", "##dist", &orbitDist, 0.1f, 0.3f, 500.0f)) { cameraState.OrbitDist = orbitDist; orb = true; }
                if (orb) host.ApplyOrbit();

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f, 0.557f, 0.576f, 1.0f));
                ImGui::TextUnformatted("Target");
                ImGui::PopStyleColor();
                ImGui::SameLine(lw);
                ImGui::SetNextItemWidth(-1.0f);
                glm::vec3 orbitTarget = cameraState.OrbitTarget;
                if (ImGui::DragFloat3("##target", glm::value_ptr(orbitTarget), 0.05f))
                    { cameraState.OrbitTarget = orbitTarget; host.ApplyOrbit(); }

                glm::vec3 cpos = viewport_camera->GetPosition();
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.357f, 0.357f, 0.376f, 1.0f));
                ImGui::Text("Pos  %.2f  %.2f  %.2f", cpos.x, cpos.y, cpos.z);
                ImGui::PopStyleColor();

                UIActive::SectionHeader("CONTROLS");
                bool followObject = cameraState.FollowObject;
                if (UIActive::Toggle("Follow Selected", &followObject))
                    cameraState.FollowObject = followObject;

                ImGui::Spacing();
                if (UIActive::DestructiveButton("Reset Camera", ImVec2(-1.0f, 0.0f)))
                    host.GetCommandStack().Push(MakeScope<CallbackCommand>(
                        [&host] { host.ResetViewportCamera(); }, [] {}, false));
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    UIActive::EndPanel();
}

} // namespace Sandbox
