#include "SceneViewLayer.h"

#include <CHEngine/EngineConfig.h>
#include <CHEngine/Mesh/Material.h>
#include <CHEngine/Render/RenderFacade.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>
#include <optional>
#include <boost/container_hash/hash.hpp>

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
            if (ImGui::IsKeyPressed(ImGuiKey_F3, false)) m_ShowProfiler = !m_ShowProfiler;

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

        ImGui::SameLine(0, 12);
        vcenter(20.0f);
        UIActive::Toggle("Profiler", &m_ShowProfiler);

        // ── Play / Pause / Stop — центр тулбара ──────────────────────────────
        {
            // Горячие клавиши
            if (!ImGui::GetIO().WantTextInput)
            {
                const bool mod = ImGui::GetIO().KeySuper || ImGui::GetIO().KeyCtrl;
                if (mod && ImGui::IsKeyPressed(ImGuiKey_P, false))
                {
                    if      (m_EditorState == EditorState::Edit)  EnterPlayMode();
                    else if (m_EditorState == EditorState::Play)  EnterPauseMode();
                    else if (m_EditorState == EditorState::Pause) ResumeFromPause();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && m_EditorState != EditorState::Edit)
                    StopPlayMode();
                if (mod && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
                    StepOneFrame();
            }

            const bool isEdit  = (m_EditorState == EditorState::Edit);
            const bool isPlay  = (m_EditorState == EditorState::Play);
            const bool isPause = (m_EditorState == EditorState::Pause);

            // Абсолютное позиционирование по центру окна (независимо от left-flow)
            const float pad    = ImGui::GetStyle().FramePadding.x;
            const float btnW   = ImGui::CalcTextSize("Pause").x + pad * 2.0f + 14.0f;
            const float stopW  = ImGui::CalcTextSize("Stop").x  + pad * 2.0f + 14.0f;
            const float stepW  = ImGui::CalcTextSize("Step").x  + pad * 2.0f + 10.0f;
            const float gap    = 4.0f;
            const float blockW = btnW + stopW + stepW + gap * 2.0f;

            ImVec2 winPos  = ImGui::GetWindowPos();
            float  winW    = ImGui::GetWindowWidth();
            float  winH    = ImGui::GetWindowHeight();
            float  screenX = winPos.x + (winW - blockW) * 0.5f;
            float  screenY = winPos.y + (winH - ImGui::GetFrameHeight()) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(screenX, screenY));

            // Play / Pause кнопка
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.50f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.62f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.38f, 0.10f, 1.0f));
            if (isPlay)
            {
                if (ImGui::Button("Pause##playctrl", ImVec2(btnW, 0)))
                    EnterPauseMode();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause  (Cmd+P)");
            }
            else
            {
                const char* label = isPause ? "Resume##playctrl" : "Play##playctrl";
                if (ImGui::Button(label, ImVec2(btnW, 0)))
                {
                    if (isEdit) EnterPlayMode();
                    else        ResumeFromPause();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(isEdit ? "Play  (Cmd+P)" : "Resume  (Cmd+P)");
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0, gap);
            vcenter(ImGui::GetFrameHeight());

            // Stop
            ImGui::BeginDisabled(isEdit);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.50f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.62f, 0.20f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.38f, 0.10f, 0.10f, 1.0f));
            if (ImGui::Button("Stop##playctrl", ImVec2(stopW, 0)))
                StopPlayMode();
            ImGui::PopStyleColor(3);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop  (Esc)");

            ImGui::SameLine(0, gap);
            vcenter(ImGui::GetFrameHeight());

            // Step
            ImGui::BeginDisabled(!isPause);
            if (ImGui::Button("Step##playctrl", ImVec2(stepW, 0)))
                StepOneFrame();
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step one frame  (Cmd+Shift+Right)");
        }

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
            { "Metal",   CHEngine::ERenderAPI::METAL  },
        };

        // Фильтруем: оставляем только поддерживаемые на этой платформе
        ApiEntry availApis[3];
        int availCount = 0;
        for (auto& e : allApis) {
            if (CHEngine::RenderAPICaps::IsAvailable(e.api))
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

        float saveSceneW = ImGui::CalcTextSize("Save Scene").x + pad * 2.0f;
        float openSceneW = ImGui::CalcTextSize("Open Scene").x + pad * 2.0f;

        float rightBlockW = saveSceneW + 4.0f
                          + openSceneW + 12.0f
                          + ImGui::CalcTextSize("|").x + 12.0f  // separator
                          + rendererComboW
                          + 8.0f
                          + ImGui::CalcTextSize(themeLabel).x + pad * 2.0f
                          + 8.0f
                          + ImGui::CalcTextSize(fpsBuf).x
                          + ImGui::GetStyle().WindowPadding.x;

        float startX = ImGui::GetWindowWidth() - rightBlockW;
        if (startX > ImGui::GetCursorPosX() + 10.0f)
        {
            ImGui::SetCursorPosX(startX);

            // Save / Open Scene
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button("Save Scene"))
                SaveScene();
            ImGui::SameLine(0, 4);
            vcenter(ImGui::GetFrameHeight());
            if (ImGui::Button("Open Scene"))
                LoadScene();

            ImGui::SameLine(0, 12);
            vcenter(20.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0, 12);

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

    ImGui::BeginDisabled(m_EditorState != EditorState::Edit);
    if (UIActive::PrimaryButton("+ Import Model", ImVec2(-1.0f, 0.0f)))
    {
        std::string path = CHEngine::FileDialog::OpenModelFile();
        if (!path.empty()) ImportModel(path);
    }
    ImGui::EndDisabled();

    ImGui::Spacing();

    // ── Кнопки добавления источников света ────────────────────────────────
    float halfW = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;
    if (ImGui::Button("+ Dir Light", ImVec2(halfW, 0.0f)))
    {
        auto handle = CHEngine::SceneSpawner::CreateLightEntity(m_Scene, "Directional Light", CHEngine::LightType::Directional);
        if (auto* entity = m_Scene.TryGetEntity(handle); entity && entity->HasComponent<CHEngine::TransformComponent>()) {
            entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform.Rotation = { -45.0f, -30.0f, 0.0f };
            m_SelectedObjectID = m_Scene.GetUUID(handle);
        }
    }
    ImGui::SameLine(0, 4);
    if (ImGui::Button("+ Point Light", ImVec2(halfW, 0.0f)))
    {
        auto handle = CHEngine::SceneSpawner::CreateLightEntity(m_Scene, "Point Light", CHEngine::LightType::Point);
        if (auto* entity = m_Scene.TryGetEntity(handle); entity && entity->HasComponent<CHEngine::TransformComponent>()) {
            entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform.Position = { 0.0f, 3.0f, 0.0f };
            m_SelectedObjectID = m_Scene.GetUUID(handle);
        }
    }
    if (ImGui::Button("+ Spot Light", ImVec2(-1.0f, 0.0f)))
    {
        auto handle = CHEngine::SceneSpawner::CreateLightEntity(m_Scene, "Spot Light", CHEngine::LightType::Spot);
        if (auto* entity = m_Scene.TryGetEntity(handle); entity && entity->HasComponent<CHEngine::TransformComponent>()) {
            auto& tr = entity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
            tr.Position = { 0.0f, 5.0f, 0.0f };
            tr.Rotation = { -90.0f, 0.0f, 0.0f };
            m_SelectedObjectID = m_Scene.GetUUID(handle);
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        ImVec4(0.282f, 0.282f, 0.290f, 0.45f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    std::optional<CHEngine::UUID> deleteID;
    size_t objectCount = 0;
    m_Scene.ForEach<CHEngine::TagComponent>([&](CHEngine::EntityHandle handle,
                                                const CHEngine::UUID& objectID,
                                                CHEngine::TagComponent& tag) {
        ++objectCount;
        bool isSelected = (objectID == m_SelectedObjectID);
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_Leaf         |
            ImGuiTreeNodeFlags_SpanAvailWidth|
            ImGuiTreeNodeFlags_FramePadding;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        // Метка типа объекта в иерархии
        const char* icon = "";
        if (const auto* entity = m_Scene.TryGetEntity(handle); entity && entity->HasComponent<CHEngine::LightComponent>()) {
            const auto type = entity->GetComponent<CHEngine::LightComponent>().LightData.Type;
            if (type == CHEngine::LightType::Directional) icon = "[D] ";
            else if (type == CHEngine::LightType::Point)  icon = "[P] ";
            else if (type == CHEngine::LightType::Spot)   icon = "[S] ";
        }

        ImGui::PushID(static_cast<int>(boost::hash<CHEngine::UUID>{}(objectID)));
        bool opened = ImGui::TreeNodeEx(
            "##object", flags, "  %s%s", icon, tag.Name.c_str());

        if (ImGui::IsItemClicked())
            m_SelectedObjectID = objectID;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Focus (F)")) FocusOnSelected();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(1.0f, 0.231f, 0.188f, 1.0f));
            if (ImGui::MenuItem("Delete")) deleteID = objectID;
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        if (opened) ImGui::TreePop();
        ImGui::PopID();
    });

    if (objectCount == 0)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f,0.686f,0.706f,1.0f));
        ImGui::TextWrapped("No objects in scene.\nImport a model to begin.");
        ImGui::PopStyleColor();
    }

    if (deleteID.has_value())
    {
        if (m_SelectedObjectID == deleteID.value()) m_SelectedObjectID = boost::uuids::nil_uuid();
        m_Scene.RemoveObject(deleteID.value());
    }

    UIActive::EndPanel();
}

// ============================================================================
//  UI — Properties / Inspector
// ============================================================================

void SceneViewLayer::DrawPropsPanel(ImVec2 pos, ImVec2 size, bool resetSize)
{
    UIActive::BeginPanel("Properties", pos, size, 0, resetSize);

    auto selectedHandle = m_Scene.TryGetEntityHandleByUUID(m_SelectedObjectID);
    if (!m_Scene.IsEntityHandleValid(selectedHandle))
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.686f,0.686f,0.706f,1.0f));
        ImGui::TextUnformatted("No object selected");
        ImGui::PopStyleColor();
        UIActive::EndPanel();
        return;
    }

    auto* selectedEntity = m_Scene.TryGetEntity(selectedHandle);
    if (!selectedEntity
        || !selectedEntity->HasComponent<CHEngine::TagComponent>()
        || !selectedEntity->HasComponent<CHEngine::TransformComponent>()
        || !selectedEntity->HasComponent<CHEngine::MeshComponent>()
        || !selectedEntity->HasComponent<CHEngine::ColorComponent>()
        || !selectedEntity->HasComponent<CHEngine::VisibilityComponent>()) {
        UIActive::EndPanel();
        return;
    }
    auto& tag = selectedEntity->GetComponent<CHEngine::TagComponent>();
    auto& transform = selectedEntity->GetComponent<CHEngine::TransformComponent>().ObjectTransform;
    auto& meshComp = selectedEntity->GetComponent<CHEngine::MeshComponent>();
    auto& colorComp = selectedEntity->GetComponent<CHEngine::ColorComponent>();
    auto& visibility = selectedEntity->GetComponent<CHEngine::VisibilityComponent>();

    // В Play/Pause — только чтение (для наблюдения за состоянием)
    const bool propsReadOnly = (m_EditorState != EditorState::Edit);
    if (propsReadOnly)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
        ImGui::TextUnformatted(m_EditorState == EditorState::Play ? "\xe2\x96\xb6 Play mode — read only"
                                                                   : "\xe2\x8f\xb8 Paused — read only");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::BeginDisabled(true);
    }

    // Name field
    char nameBuf[256];
    std::strncpy(nameBuf, tag.Name.c_str(), sizeof(nameBuf));
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf)))
        tag.Name = nameBuf;

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
            m_TransformBeforeDrag = transform;
        if (ImGui::IsItemDeactivatedAfterEdit())
            m_UndoStack.PushTransform(&m_Scene, m_SelectedObjectID, m_TransformBeforeDrag);
    };
    row("Position", "##pos", transform.Position, 0.05f);
    row("Rotation", "##rot", transform.Rotation, 0.5f);
    row("Scale",    "##scl", transform.Scale,    0.01f, 0.001f, 1000.0f);

    ImGui::Spacing();
    if (ImGui::Button("Reset Transform", ImVec2(-1.0f, 0.0f)))
    {
        m_UndoStack.PushTransform(&m_Scene, m_SelectedObjectID, transform);
        transform = {};
    }

    // MATERIAL
    UIActive::SectionHeader("MATERIAL");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::ColorEdit4("##color", glm::value_ptr(colorComp.Color));
    ImGui::Spacing();
    {
        bool before = visibility.Visible;
        if (UIActive::Toggle("Visible", &visibility.Visible) && visibility.Visible != before)
            m_UndoStack.PushVisibility(&m_Scene, m_SelectedObjectID, before);
    }

    // ── Текстуры (по одному материалу на сабмеш) ─────────────────────────
    for (size_t mi = 0; mi < meshComp.Meshes.size(); ++mi)
    {
        ImGui::PushID(static_cast<int>(mi));
        if (meshComp.Meshes.size() > 1)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::Text("Mesh %zu", mi);
            ImGui::PopStyleColor();
        }

        auto& subMesh = meshComp.Meshes[mi];
        if (!subMesh.Mat)
            subMesh.Mat = CHEngine::MaterialInstance::FromBase(
                std::make_shared<CHEngine::Material>(m_MeshShader));
        CHEngine::MaterialInstance& mat = *subMesh.Mat;

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted("Textures");
        ImGui::PopStyleColor();

    // Diffuse текстура
    {
            char diffBuf[512];
            std::string effDiff = mat.EffectiveDiffuseMapPath();
            const char* dSrc = effDiff.empty() ? "(none)" : effDiff.c_str();
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
                    CHEngine::TextureHandle d0, s0;
                    mat.ResolveTextures(d0, s0);
                    if (d0.IsValid())
                        CHEngine::RenderFacade::DestroyTexture(d0);
                    if (mat.m_Material)
                    {
                        mat.m_Material->DiffuseMap = CHEngine::TextureHandle{};
                        mat.m_Material->DiffuseMapPath.clear();
                    }
                    mat.DiffuseMap = CHEngine::RenderFacade::CreateTextureFromFile(path);
                    mat.DiffuseMapPath = mat.DiffuseMap.IsValid() ? path : "";
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Выбрать текстуру");
            ImGui::SameLine(0, 4);
            if (ImGui::Button("X##dif", ImVec2(20, 0)))
            {
                CHEngine::TextureHandle d0, s0;
                mat.ResolveTextures(d0, s0);
                if (d0.IsValid())
                    CHEngine::RenderFacade::DestroyTexture(d0);
                mat.DiffuseMap = CHEngine::TextureHandle{};
                mat.DiffuseMapPath.clear();
                if (mat.m_Material)
                {
                    mat.m_Material->DiffuseMap = CHEngine::TextureHandle{};
                    mat.m_Material->DiffuseMapPath.clear();
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Убрать текстуру");
    }

    // Specular текстура
    {
            char specBuf[512];
            std::string effSpec = mat.EffectiveSpecularMapPath();
            const char* sSrc = effSpec.empty() ? "(none)" : effSpec.c_str();
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
                    CHEngine::TextureHandle d0, s0;
                    mat.ResolveTextures(d0, s0);
                    if (s0.IsValid())
                        CHEngine::RenderFacade::DestroyTexture(s0);
                    if (mat.m_Material)
                    {
                        mat.m_Material->SpecularMap = CHEngine::TextureHandle{};
                        mat.m_Material->SpecularMapPath.clear();
                    }
                    mat.SpecularMap = CHEngine::RenderFacade::CreateTextureFromFile(path);
                    mat.SpecularMapPath = mat.SpecularMap.IsValid() ? path : "";
                }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Выбрать specular карту");
            ImGui::SameLine(0, 4);
            if (ImGui::Button("X##spec", ImVec2(20, 0)))
            {
                CHEngine::TextureHandle d0, s0;
                mat.ResolveTextures(d0, s0);
                if (s0.IsValid())
                    CHEngine::RenderFacade::DestroyTexture(s0);
                mat.SpecularMap = CHEngine::TextureHandle{};
                mat.SpecularMapPath.clear();
                if (mat.m_Material)
                {
                    mat.m_Material->SpecularMap = CHEngine::TextureHandle{};
                    mat.m_Material->SpecularMapPath.clear();
                }
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

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
    ImGui::TextUnformatted("Specular scale");
    ImGui::PopStyleColor();
    ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::DragFloat("##specScale", &mat.SpecularScale, 0.02f, 0.0f, 4.0f, "%.2f");

        ImGui::PopID();
    }

    // LIGHT (показывать только для источников света)
    if (selectedEntity->HasComponent<CHEngine::LightComponent>())
    {
        auto* lightComp = &selectedEntity->GetComponent<CHEngine::LightComponent>();
        auto& lightData = lightComp->LightData;
        UIActive::SectionHeader("LIGHT");

        const char* lightTypes[] = { "Directional", "Point", "Spot" };
        int ltIdx = static_cast<int>(lightData.Type);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##lightType", &ltIdx, lightTypes, 3))
            lightData.Type = static_cast<CHEngine::LightType>(ltIdx);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted("Color");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::ColorEdit3("##lightColor", glm::value_ptr(lightData.Color));

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
        ImGui::TextUnformatted("Intensity");
        ImGui::PopStyleColor();
        ImGui::SameLine(labelW);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##lightIntensity", &lightData.Intensity, 0.05f, 0.0f, 100.0f, "%.2f");

        if (lightData.Type != CHEngine::LightType::Directional)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Range");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightRange", &lightData.Range, 0.1f, 0.1f, 1000.0f, "%.1f");
        }

        if (lightData.Type == CHEngine::LightType::Spot)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Inner");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightInner", &lightData.InnerCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
            ImGui::TextUnformatted("Outer");
            ImGui::PopStyleColor();
            ImGui::SameLine(labelW);
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##lightOuter", &lightData.OuterCone, 0.5f, 0.0f, 89.0f, "%.1f\xc2\xb0");
        }

        ImGui::Spacing();
    }

    // INFO
    UIActive::SectionHeader("INFO");
    uint32_t tv = 0, ti = 0;
    for (auto& m : meshComp.Meshes)
    {
        tv += static_cast<uint32_t>(m.GetVertices().size());
        ti += static_cast<uint32_t>(m.GetIndices().size());
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.557f,0.557f,0.576f,1.0f));
    ImGui::Text("Meshes      %zu",  meshComp.Meshes.size());
    ImGui::Text("Vertices    %u",   tv);
    ImGui::Text("Triangles   %u",   ti / 3);
    ImGui::PopStyleColor();

    ImGui::Spacing();
    if (ImGui::Button("Focus Camera  (F)", ImVec2(-1.0f, 0.0f)))
        FocusOnSelected();

    if (propsReadOnly)
        ImGui::EndDisabled();

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

// ============================================================================
//  UI — Profiler overlay
// ============================================================================

void SceneViewLayer::DrawProfilerPanel()
{
    if (!m_ShowProfiler) return;

    ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Profiler", &m_ShowProfiler))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Reset"))
        CHEngine::Profiler::Reset();
    ImGui::SameLine();
    ImGui::TextDisabled("(F3 to toggle)");

    const auto& stats = CHEngine::Profiler::GetFrameStats();

    if (ImGui::BeginTable("##profiler_table", 5,
        ImGuiTableFlags_Sortable      |
        ImGuiTableFlags_RowBg         |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY       |
        ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name",     ImGuiTableColumnFlags_DefaultSort,              0.0f, 0);
        ImGui::TableSetupColumn("Calls",    ImGuiTableColumnFlags_PreferSortDescending,     0.0f, 1);
        ImGui::TableSetupColumn("Total ms", ImGuiTableColumnFlags_PreferSortDescending,     0.0f, 2);
        ImGui::TableSetupColumn("Avg ms",   ImGuiTableColumnFlags_PreferSortDescending,     0.0f, 3);
        ImGui::TableSetupColumn("Max ms",   ImGuiTableColumnFlags_PreferSortDescending,     0.0f, 4);
        ImGui::TableHeadersRow();

        struct Entry {
            const char* name;
            int    calls;
            double totalMs;
            double avgMs;
            double maxMs;
        };
        std::vector<Entry> entries;
        entries.reserve(stats.size());
        for (auto& [name, stat] : stats)
        {
            double avg = stat.calls > 0 ? stat.totalMs / stat.calls : 0.0;
            entries.push_back({ name, stat.calls, stat.totalMs, avg, stat.maxMs });
        }

        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
        {
            if (specs->SpecsDirty && specs->SpecsCount > 0)
            {
                const auto& spec = specs->Specs[0];
                std::sort(entries.begin(), entries.end(),
                    [&](const Entry& a, const Entry& b)
                    {
                        int cmp = 0;
                        switch (spec.ColumnUserID)
                        {
                        case 0: cmp = std::strcmp(a.name, b.name); break;
                        case 1: cmp = (a.calls   < b.calls)   ? -1 : (a.calls   > b.calls)   ? 1 : 0; break;
                        case 2: cmp = (a.totalMs  < b.totalMs) ? -1 : (a.totalMs  > b.totalMs) ? 1 : 0; break;
                        case 3: cmp = (a.avgMs    < b.avgMs)   ? -1 : (a.avgMs    > b.avgMs)   ? 1 : 0; break;
                        case 4: cmp = (a.maxMs    < b.maxMs)   ? -1 : (a.maxMs    > b.maxMs)   ? 1 : 0; break;
                        }
                        return (spec.SortDirection == ImGuiSortDirection_Ascending) ? cmp < 0 : cmp > 0;
                    });
                specs->SpecsDirty = false;
            }
        }

        for (auto& e : entries)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(e.name);
            ImGui::TableNextColumn(); ImGui::Text("%d", e.calls);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", e.totalMs);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", e.avgMs);
            ImGui::TableNextColumn(); ImGui::Text("%.3f", e.maxMs);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

