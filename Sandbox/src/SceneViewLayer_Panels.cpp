#include "SceneViewLayer.h"

#include <glm/gtc/type_ptr.hpp>
#include <cstring>

// ============================================================================
//  UI — Toolbar
// ============================================================================

void SceneViewLayer::DrawToolbar(ImVec2 pos, ImVec2 size)
{
    UIActive::BeginToolbar(pos, size);

    // Vertical-centering helper — nudges each item so its visual centre
    // aligns with the toolbar's horizontal midline.
    {
        const float winH    = ImGui::GetWindowHeight();
        const float padY    = 9.0f;
        const float startY  = ImGui::GetCursorPosY();
        const float centerY = startY + (winH - padY * 2.0f) * 0.5f;

        struct VC {
            float cy;
            void operator()(float itemH) const {
                ImGui::SetCursorPosY(cy - itemH * 0.5f);
            }
        } vcenter{ centerY };

        // Hotkeys T / R / S / Undo
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_T, false)) m_GizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) m_GizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) m_GizmoOperation = ImGuizmo::SCALE;

            // Cmd+Z (macOS) или Alt+Z (Windows/Linux)
            const bool undoMod = ImGui::GetIO().KeySuper   // Cmd на Mac
                              || ImGui::GetIO().KeyAlt;    // Alt на Windows
            if (undoMod && ImGui::IsKeyPressed(ImGuiKey_Z, false) && m_UndoStack.CanUndo())
                m_UndoStack.Undo();
        }

        // Gizmo segmented control
        const char* gizmoLabels[] = { "Translate", "Rotate", "Scale" };
        int gizmoSel = (m_GizmoOperation == ImGuizmo::TRANSLATE) ? 0
                     : (m_GizmoOperation == ImGuizmo::ROTATE   ) ? 1 : 2;

        vcenter(ImGui::GetFrameHeight());
        if (UIActive::SegmentedControl("##gizmo", gizmoLabels, 3, &gizmoSel,
                                       ImVec2(272.0f, 0.0f)))
        {
            m_GizmoOperation = (gizmoSel == 0) ? ImGuizmo::TRANSLATE
                             : (gizmoSel == 1) ? ImGuizmo::ROTATE
                                               : ImGuizmo::SCALE;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("T  Translate\nR  Rotate\nS  Scale");

        ImGui::SameLine(0, 20);
        vcenter(20.0f);
        if (UIActive::Toggle("Local", &m_LocalMode))
            m_GizmoMode = m_LocalMode ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        ImGui::SameLine(0, 20);
        vcenter(20.0f);
        UIActive::Toggle("Grid", &m_ShowGrid);

        // Scene save/load buttons
        ImGui::SameLine(0, 12);
        vcenter(20.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine(0, 12);
        vcenter(ImGui::GetFrameHeight());
        if (ImGui::Button("Save Scene"))
            SaveScene();
        ImGui::SameLine(0, 4);
        vcenter(ImGui::GetFrameHeight());
        if (ImGui::Button("Open Scene"))
            LoadScene();

        // Snap hint
        {
            bool   shiftHeld = ImGui::GetIO().KeyShift;
            ImVec4 col = shiftHeld
                ? ImVec4(0.20f, 0.60f, 1.00f, 1.0f)
                : ImVec4(0.43f, 0.43f, 0.45f, 1.0f);
            ImGui::SameLine(0, 20);
            vcenter(ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(shiftHeld ? "\xe2\x87\xa7 Snap ON" : "\xe2\x87\xa7 Snap");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Hold Shift to snap:\n"
                                  "  Translate  1 unit\n"
                                  "  Rotate     45\xc2\xb0\n"
                                  "  Scale      0.1 unit");
        }

        // Right side: Theme toggle + FPS
        float       fps    = ImGui::GetIO().Framerate;
        char        fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f fps", fps);
        const char* themeLabel = (UIActive::g_Theme == AppTheme::RetroOS) ? "Theme: Retro" : "Theme: Dark";
        const float pad = ImGui::GetStyle().FramePadding.x;
        float rightBlockW = ImGui::CalcTextSize(themeLabel).x + pad * 2.0f
                          + 8.0f
                          + ImGui::CalcTextSize(fpsBuf).x
                          + ImGui::GetStyle().WindowPadding.x;

        float startX = ImGui::GetWindowWidth() - rightBlockW;
        if (startX > ImGui::GetCursorPosX() + 10.0f)
        {
            ImGui::SetCursorPosX(startX);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button(themeLabel))
            {
                AppTheme next = (UIActive::g_Theme == AppTheme::RetroOS)
                              ? AppTheme::Dark : AppTheme::RetroOS;
                UIActive::SetTheme(next);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Switch UI theme");
            ImGui::SameLine(0, 8);
            vcenter(ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted(fpsBuf);
            ImGui::PopStyleColor();
        }

    } // end vcenter scope

    UIActive::EndToolbar();
}

// ============================================================================
//  UI — Scene Hierarchy
// ============================================================================

void SceneViewLayer::DrawScenePanel(ImVec2 pos, ImVec2 size, bool resetSize)
{
    UIActive::BeginPanel("Scene", pos, size, 0, resetSize);

    if (UIActive::PrimaryButton("+ Import Model", ImVec2(-1.0f, 0.0f)))
    {
        std::string path = CHEngine::FileDialog::OpenModelFile();
        if (!path.empty()) ImportModel(path);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        ImVec4(0.282f, 0.282f, 0.290f, 0.45f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    uint32_t deleteID = 0;
    for (auto& obj : m_Scene.GetObjects())
    {
        bool isSelected = (obj->ID == m_SelectedObjectID);
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_Leaf         |
            ImGuiTreeNodeFlags_SpanAvailWidth|
            ImGuiTreeNodeFlags_FramePadding;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        bool opened = ImGui::TreeNodeEx(
            (void*)(intptr_t)obj->ID, flags, "  %s", obj->Name.c_str());

        if (ImGui::IsItemClicked())
            m_SelectedObjectID = obj->ID;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Focus (F)")) FocusOnSelected();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(1.0f, 0.231f, 0.188f, 1.0f));
            if (ImGui::MenuItem("Delete")) deleteID = obj->ID;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        if (opened) ImGui::TreePop();
    }

    if (m_Scene.GetObjects().empty())
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f,0.686f,0.706f,1.0f));
        ImGui::TextWrapped("No objects in scene.\nImport a model to begin.");
        ImGui::PopStyleColor();
    }

    if (deleteID > 0)
    {
        if (m_SelectedObjectID == deleteID) m_SelectedObjectID = 0;
        m_Scene.RemoveObject(deleteID);
    }

    UIActive::EndPanel();
}

// ============================================================================
//  UI — Properties / Inspector
// ============================================================================

void SceneViewLayer::DrawPropsPanel(ImVec2 pos, ImVec2 size, bool resetSize)
{
    UIActive::BeginPanel("Properties", pos, size, 0, resetSize);

    CHEngine::SceneObject* sel = m_Scene.FindByID(m_SelectedObjectID);
    if (!sel)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f,0.686f,0.706f,1.0f));
        ImGui::TextUnformatted("No object selected");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    // Name field
    char nameBuf[256];
    std::strncpy(nameBuf, sel->Name.c_str(), sizeof(nameBuf));
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
        sel->Name = nameBuf;

    // TRANSFORM
    UIActive::SectionHeader("TRANSFORM");

    const float labelW = 64.0f;
    // Для undo: захватить трансформ при начале редактирования слайдера
    auto row = [&](const char* label, const char* id, glm::vec3& vec,
                   float speed, float mn = -FLT_MAX, float mx = FLT_MAX)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat3(id, glm::value_ptr(vec), speed, mn, mx);
        if (ImGui::IsItemActivated())
            m_TransformBeforeDrag = sel->ObjectTransform;
        if (ImGui::IsItemDeactivatedAfterEdit())
            m_UndoStack.PushTransform(&m_Scene, sel->ID, m_TransformBeforeDrag);
    };
    row("Position", "##pos", sel->ObjectTransform.Position, 0.05f);
    row("Rotation", "##rot", sel->ObjectTransform.Rotation, 0.5f);
    row("Scale",    "##scl", sel->ObjectTransform.Scale,    0.01f, 0.001f, 1000.0f);

    ImGui::Spacing();
    if (ImGui::Button("Reset Transform", ImVec2(-1.0f, 0.0f)))
    {
        m_UndoStack.PushTransform(&m_Scene, sel->ID, sel->ObjectTransform);
        sel->ObjectTransform = {};
    }

    // MATERIAL
    UIActive::SectionHeader("MATERIAL");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::ColorEdit4("##color", glm::value_ptr(sel->Color));
    ImGui::Spacing();
    {
        bool before = sel->Visible;
        if (UIActive::Toggle("Visible", &sel->Visible) && sel->Visible != before)
            m_UndoStack.PushVisibility(&m_Scene, sel->ID, before);
    }

    // INFO
    UIActive::SectionHeader("INFO");
    uint32_t tv = 0, ti = 0;
    for (auto& m : sel->Meshes)
    {
        tv += static_cast<uint32_t>(m.GetVertices().size());
        ti += static_cast<uint32_t>(m.GetIndices().size());
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
    ImGui::Text("Meshes      %zu",  sel->Meshes.size());
    ImGui::Text("Vertices    %u",   tv);
    ImGui::Text("Triangles   %u",   ti / 3);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    if (ImGui::Button("Focus Camera  (F)", ImVec2(-1.0f, 0.0f)))
        FocusOnSelected();

    UIActive::EndPanel();
}

// ============================================================================
//  UI — Camera settings
// ============================================================================

void SceneViewLayer::DrawCameraPanel(ImVec2 pos, ImVec2 size, bool resetSize)
{
    UIActive::BeginPanel("Camera", pos, size, 0, resetSize);

    UIActive::SectionHeader("VIEW");

    if (UIActive::PrimaryButton("Perspective", ImVec2(-1.0f, 0.0f)))
        { SetViewPreset(-90.0f, -15.0f); m_Camera.SetFOV(45.0f); }

    ImGui::Spacing();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    float hw = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;

    struct ViewPreset { const char* name; float yaw; float pitch; };
    ViewPreset presets[] = {
        { "Front",  -90.0f,   0.0f }, { "Back",    90.0f,   0.0f },
        { "Top",    -90.0f, -89.9f }, { "Bottom", -90.0f,  89.9f },
        { "Right",  180.0f,   0.0f }, { "Left",     0.0f,   0.0f },
    };
    for (int i = 0; i < 6; i++)
    {
        if (i % 2 != 0) ImGui::SameLine();
        if (ImGui::Button(presets[i].name, ImVec2(hw, 0)))
            SetViewPreset(presets[i].yaw, presets[i].pitch);
    }
    ImGui::PopStyleVar();

    UIActive::SectionHeader("ORBIT");

    bool        orb = false;
    const float lw  = 64.0f;

    auto sliderRow = [&](const char* label, const char* id,
                          float* val, float mn, float mx, const char* fmt)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(lw);
        ImGui::SetNextItemWidth(-1.0f);
        return ImGui::SliderFloat(id, val, mn, mx, fmt);
    };
    auto dragRow = [&](const char* label, const char* id,
                        float* val, float spd, float mn, float mx)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(lw);
        ImGui::SetNextItemWidth(-1.0f);
        return ImGui::DragFloat(id, val, spd, mn, mx, "%.2f");
    };

    float yaw   = m_Camera.GetYaw();
    float pitch = m_Camera.GetPitch();
    float fov   = m_Camera.GetFOV();

    if (sliderRow("Yaw",   "##yaw",  &yaw,   -180.0f, 180.0f, "%.1f°"))
        { m_Camera.SetYaw(yaw);     orb = true; }
    if (sliderRow("Pitch", "##pitch",&pitch,   -89.0f,  89.0f, "%.1f°"))
        { m_Camera.SetPitch(pitch); orb = true; }
    if (sliderRow("FOV",   "##fov",  &fov,     10.0f,  120.0f, "%.1f°"))
        m_Camera.SetFOV(fov);
    if (dragRow  ("Dist",  "##dist", &m_OrbitDist, 0.1f, 0.3f, 500.0f))
        orb = true;

    if (orb) ApplyOrbit();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
    ImGui::TextUnformatted("Target");
    ImGui::PopStyleColor();
    ImGui::SameLine(lw);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::DragFloat3("##target", glm::value_ptr(m_OrbitTarget), 0.05f))
        ApplyOrbit();

    glm::vec3 cpos = m_Camera.GetPosition();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.357f,0.357f,0.376f,1.0f));
    ImGui::Text("Pos  %.2f  %.2f  %.2f", cpos.x, cpos.y, cpos.z);
    ImGui::PopStyleColor();

    UIActive::SectionHeader("CONTROLS");
    UIActive::Toggle("Follow Selected", &m_FollowObject);

    ImGui::Spacing();
    if (UIActive::DestructiveButton("Reset Camera", ImVec2(-1.0f, 0.0f)))
    {
        m_OrbitTarget = { 0.0f, 0.0f, 0.0f };
        m_OrbitDist   = 8.0f;
        m_Camera.SetYaw(-90.0f);
        m_Camera.SetPitch(-15.0f);
        m_Camera.SetFOV(45.0f);
        ApplyOrbit();
    }

    UIActive::EndPanel();
}
