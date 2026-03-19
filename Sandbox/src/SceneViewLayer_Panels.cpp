#include "SceneViewLayer.h"

#include <CHEngine/EngineConfig.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <cmath>

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

        // Right side: Renderer selector + Theme toggle + FPS
        float       fps    = ImGui::GetIO().Framerate;
        char        fpsBuf[32];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f fps", fps);
        const char* themeLabel = (UIActive::g_Theme == AppTheme::RetroOS) ? "Theme: Retro" : "Theme: Dark";
        const float pad = ImGui::GetStyle().FramePadding.x;

        // Renderer combo — только платформно-совместимые API
        struct ApiEntry { const char* name; CHEngine::ERenderAPI api; };
        static const ApiEntry allApis[] = {
            { "OpenGL", CHEngine::ERenderAPI::OPENGL },
            { "Vulkan",  CHEngine::ERenderAPI::VULKAN  },
            { "Metal",   CHEngine::ERenderAPI::METALL  },
        };

        // Фильтруем: оставляем только поддерживаемые на этой платформе
        ApiEntry availApis[3];
        int availCount = 0;
        for (auto& e : allApis) {
            if (CHEngine::EngineConfig::IsPlatformSupported(e.api))
                availApis[availCount++] = e;
        }

        CHEngine::ERenderAPI curApi = CHEngine::Application::Get().GetRenderAPIType();
        int apiIdx = 0;
        for (int i = 0; i < availCount; ++i) {
            if (availApis[i].api == curApi) { apiIdx = i; break; }
        }

        char rendererLabel[32];
        snprintf(rendererLabel, sizeof(rendererLabel), "API: %s", availApis[apiIdx].name);
        float rendererComboW = ImGui::CalcTextSize(rendererLabel).x + pad * 2.0f + 8.0f;

        float rightBlockW = rendererComboW
                          + 8.0f
                          + ImGui::CalcTextSize(themeLabel).x + pad * 2.0f
                          + 8.0f
                          + ImGui::CalcTextSize(fpsBuf).x
                          + ImGui::GetStyle().WindowPadding.x;

        float startX = ImGui::GetWindowWidth() - rightBlockW;
        if (startX > ImGui::GetCursorPosX() + 10.0f)
        {
            ImGui::SetCursorPosX(startX);

            // Renderer dropdown
            vcenter(ImGui::GetFrameHeight());
            ImGui::SetNextItemWidth(rendererComboW);
            if (ImGui::BeginCombo("##renderer", rendererLabel, ImGuiComboFlags_NoArrowButton))
            {
                for (int i = 0; i < availCount; ++i) {
                    bool selected = (i == apiIdx);
                    if (ImGui::Selectable(availApis[i].name, selected)) {
                        if (i != apiIdx) {
                            CHEngine::EngineConfig::SaveRendererPreference(availApis[i].api);
                            AutoSaveForRestart();
                            CHEngine::Application::Get().RequestRestart();
                        }
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Switch render API (restarts engine)");

            ImGui::SameLine(0, 8);
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

    // ── Кнопки добавления источников света ────────────────────────────────
    float halfW = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
    if (ImGui::Button("+ Dir Light", ImVec2(halfW, 0.0f)))
    {
        auto* light = m_Scene.AddLight("Directional Light", CHEngine::LightType::Directional);
        if (light) {
            light->ObjectTransform.Rotation = { -45.0f, -30.0f, 0.0f };
            m_SelectedObjectID = light->ID;
        }
    }
    ImGui::SameLine(0, 4);
    if (ImGui::Button("+ Point Light", ImVec2(halfW, 0.0f)))
    {
        auto* light = m_Scene.AddLight("Point Light", CHEngine::LightType::Point);
        if (light) {
            light->ObjectTransform.Position = { 0.0f, 3.0f, 0.0f };
            m_SelectedObjectID = light->ID;
        }
    }
    if (ImGui::Button("+ Spot Light", ImVec2(-1.0f, 0.0f)))
    {
        auto* light = m_Scene.AddLight("Spot Light", CHEngine::LightType::Spot);
        if (light) {
            light->ObjectTransform.Position = { 0.0f, 5.0f, 0.0f };
            light->ObjectTransform.Rotation = { -90.0f, 0.0f, 0.0f };
            m_SelectedObjectID = light->ID;
        }
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

        // Метка типа объекта в иерархии
        const char* icon = "";
        if (obj->LightData.Type == CHEngine::LightType::Directional) icon = "[D] ";
        else if (obj->LightData.Type == CHEngine::LightType::Point)  icon = "[P] ";
        else if (obj->LightData.Type == CHEngine::LightType::Spot)   icon = "[S] ";

        bool opened = ImGui::TreeNodeEx(
            (void*)(intptr_t)obj->ID, flags, "  %s%s", icon, obj->Name.c_str());

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

    // ── Текстуры меша ─────────────────────────────────────────────────────
    for (size_t mi = 0; mi < sel->Meshes.size(); ++mi)
    {
        auto& mat = sel->Meshes[mi].Mat;

        // Уникальный ImGui ID для каждого меша (избегает коллизии виджетов)
        ImGui::PushID(static_cast<int>(mi));

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        if (sel->Meshes.size() > 1)
            ImGui::Text("Mesh %zu", mi);
        else
            ImGui::TextUnformatted("Textures");
        ImGui::PopStyleColor();

        // Diffuse текстура
        {
            char diffBuf[512];
            const char* dSrc = mat.DiffuseMapPath.empty() ? "(none)" : mat.DiffuseMapPath.c_str();
            std::strncpy(diffBuf, dSrc, sizeof(diffBuf));
            diffBuf[sizeof(diffBuf) - 1] = '\0';

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Diffuse");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            float fieldW = ImGui::GetContentRegionAvail().x - 52.0f;
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputText("##diffPath", diffBuf, sizeof(diffBuf),
                             ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine(0, 4);
            if (ImGui::Button("...##dif", ImVec2(24, 0)))
            {
                std::string path = CHEngine::FileDialog::OpenImageFile();
                if (!path.empty())
                {
                    if (mat.DiffuseMap.IsValid())
                        m_Resources.DestroyTexture(mat.DiffuseMap);
                    mat.DiffuseMap = m_Resources.CreateTextureFromFile(path);
                    mat.DiffuseMapPath = mat.DiffuseMap.IsValid() ? path : "";
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Выбрать текстуру");
            ImGui::SameLine(0, 4);
            if (ImGui::Button("X##dif", ImVec2(20, 0)) && mat.DiffuseMap.IsValid())
            {
                m_Resources.DestroyTexture(mat.DiffuseMap);
                mat.DiffuseMap = CHEngine::TextureHandle::Invalid();
                mat.DiffuseMapPath.clear();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Убрать текстуру");
        }

        // Specular текстура
        {
            char specBuf[512];
            const char* sSrc = mat.SpecularMapPath.empty() ? "(none)" : mat.SpecularMapPath.c_str();
            std::strncpy(specBuf, sSrc, sizeof(specBuf));
            specBuf[sizeof(specBuf) - 1] = '\0';

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Specular");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            float fieldW = ImGui::GetContentRegionAvail().x - 52.0f;
            ImGui::SetNextItemWidth(fieldW);
            ImGui::InputText("##specPath", specBuf, sizeof(specBuf),
                             ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine(0, 4);
            if (ImGui::Button("...##spec", ImVec2(24, 0)))
            {
                std::string path = CHEngine::FileDialog::OpenImageFile();
                if (!path.empty())
                {
                    if (mat.SpecularMap.IsValid())
                        m_Resources.DestroyTexture(mat.SpecularMap);
                    mat.SpecularMap = m_Resources.CreateTextureFromFile(path);
                    mat.SpecularMapPath = mat.SpecularMap.IsValid() ? path : "";
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Выбрать specular карту");
            ImGui::SameLine(0, 4);
            if (ImGui::Button("X##spec", ImVec2(20, 0)) && mat.SpecularMap.IsValid())
            {
                m_Resources.DestroyTexture(mat.SpecularMap);
                mat.SpecularMap = CHEngine::TextureHandle::Invalid();
                mat.SpecularMapPath.clear();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Убрать specular карту");
        }

        // Shininess
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted("Shininess");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##shin", &mat.Shininess, 0.5f, 1.0f, 256.0f, "%.1f");

        ImGui::PopID();
    }

    // LIGHT (показывать только для источников света)
    if (sel->LightData.Type != CHEngine::LightType::None)
    {
        UIActive::SectionHeader("LIGHT");

        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int ltIdx = static_cast<int>(sel->LightData.Type);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##lightType", &ltIdx, lightTypes, 3))
            sel->LightData.Type = static_cast<CHEngine::LightType>(ltIdx);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted("Color");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::ColorEdit3("##lightColor", glm::value_ptr(sel->LightData.Color));

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted("Intensity");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##lightIntensity", &sel->LightData.Intensity, 0.05f, 0.0f, 100.0f, "%.2f");

        if (sel->LightData.Type != CHEngine::LightType::Directional)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Range");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightRange", &sel->LightData.Range, 0.1f, 0.1f, 1000.0f, "%.1f");
        }

        if (sel->LightData.Type == CHEngine::LightType::Spot)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Inner");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightInner", &sel->LightData.InnerCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Outer");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightOuter", &sel->LightData.OuterCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");
        }

        ImGui::Spacing();
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

