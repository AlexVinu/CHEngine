#include "UvEditorPanel.h"
#include "EditorContext.h"
#include "EditorWorldContext.h"

#include "CHEngine/Mesh/MeshRef.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Utils/MathUtils.h"

#include <CHEngine/Application.h>

#include <imgui_internal.h>
#include <algorithm>



namespace Sandbox {

static std::vector<std::vector<int>> BuildUVGroups(const std::vector<CHEngine::Vertex>& verts, float epsilon = 0.0001f)
{
    std::vector<std::vector<int>> groups;
    std::vector<bool> visited(verts.size(), false);
    for (size_t i = 0; i < verts.size(); ++i)
    {
        if (visited[i]) continue;
        std::vector<int> group;
        group.push_back(static_cast<int>(i));
        visited[i] = true;
        for (size_t j = i + 1; j < verts.size(); ++j)
        {
            if (visited[j]) continue;
            float dx = verts[i].TexCoords.x - verts[j].TexCoords.x;
            float dy = verts[i].TexCoords.y - verts[j].TexCoords.y;
            if ((dx * dx + dy * dy) < epsilon)
            {
                group.push_back(static_cast<int>(j));
                visited[j] = true;
            }
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

void UvEditorPanel::Draw(EditorContext& ctx)
{
    // Window is always created (tiling system provides pos/size via SetNextWindowPos/Size)
    if (!ImGui::Begin("UV Editor"))
    {
        ImGui::End();
        return;
    }

    EditorWorldContext* sessionRef = ctx.ActiveCtx();
    EditorWorldContext& session = *sessionRef;
    auto scene = session.EditorScene;

    // Show hint when nothing useful to display
    auto showHint = [](const char* msg)
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 textSz = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos(ImVec2(
            (avail.x - textSz.x) * 0.5f,
            (avail.y - textSz.y) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.52f, 1.0f));
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
    };

    if (!scene)                              { showHint("No scene loaded");            ImGui::End(); return; }

    CHEngine::EntityHandle sel = session.SelectedEntity;
    if (!scene->IsEntityHandleValid(sel))    { showHint("No object selected");         ImGui::End(); return; }

    CHEngine::Entity* entity = scene->TryGetEntity(sel);
    if (!entity)                             { showHint("No object selected");         ImGui::End(); return; }
    if (!entity->HasComponent<CHEngine::MeshComponent>())
                                             { showHint("Selected object has no mesh");ImGui::End(); return; }

    CHEngine::MeshComponent& meshComp = entity->GetComponent<CHEngine::MeshComponent>();
    if (!meshComp.Mesh.IsValid())            { showHint("Mesh has no geometry");       ImGui::End(); return; }

    CHEngine::MeshRef& meshRef = meshComp.Mesh;
    CHEngine::MeshLoader* meshLoader = CHEngine::Application::Get().Resources().GetMeshLoader();
    const CHEngine::MeshGpuRecord* gpuRec = meshLoader->GetGpuRecord(meshRef.Handle());
    if (!gpuRec || gpuRec->vertices.empty() || gpuRec->indices.empty() || gpuRec->subMeshes.empty())
                                             { showHint("Mesh has no geometry");       ImGui::End(); return; }
    // UV editor operates on the first submesh only for now.
    const uint32_t kEditedSubmesh = 0u;
    const auto& vertices = gpuRec->vertices;
    const auto& indices  = gpuRec->indices;

    // --- Get diffuse texture for background ---
    CHEngine::TextureHandle diffuseTex;
    auto editedMat = meshRef.IsValid() ? meshRef->GetMaterial(kEditedSubmesh) : nullptr;
    if (editedMat)
    {
        CHEngine::TextureHandle spec;
        editedMat->ResolveTextures(diffuseTex, spec);
    }

    const float canvasW = ImGui::GetContentRegionAvail().x;
    const float canvasH = ImGui::GetContentRegionAvail().y;
    if (canvasW < 1.0f || canvasH < 1.0f)
    {
        ImGui::End();
        return;
    }

    const ImVec2 canvasSize(canvasW, canvasH);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##uv_canvas", canvasSize);
    const bool isHovered = ImGui::IsItemHovered();
    const bool isActive  = ImGui::IsItemActive();
    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    const bool mouseDown = ImGui::GetIO().MouseDown[0];
    const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;

    const ImVec2 canvasEnd(canvasOrigin.x + canvasW, canvasOrigin.y + canvasH);

    // --- 1. Background: texture or dark fill ---
    bool hasTexture = false;
    if (diffuseTex.IsValid())
    {
        CHEngine::IRenderFactory* factory = CHEngine::Application::Get().Render().GetRenderFactory();
        if (factory)
        {
            uint64_t nativeID = factory->GetTextureNativeID(diffuseTex);
            if (nativeID != 0)
            {
                const bool flipV = factory->ImGuiImageNeedsVFlip();
                const ImVec2 uv0 = flipV ? ImVec2(0, 1) : ImVec2(0, 0);
                const ImVec2 uv1 = flipV ? ImVec2(1, 0) : ImVec2(1, 1);
                dl->AddImage(static_cast<ImTextureID>(nativeID),
                             canvasOrigin, canvasEnd, uv0, uv1);
                hasTexture = true;
            }
        }
    }

    if (!hasTexture)
        dl->AddRectFilled(canvasOrigin, canvasEnd, IM_COL32(30, 30, 30, 255));

    // --- 2. Dark overlay over full canvas (dim outside UV islands) ---
    dl->AddRectFilled(canvasOrigin, canvasEnd, IM_COL32(0, 0, 0, 140));

    // --- 3. Bright fill for each UV triangle (lighten face regions) ---
    auto uvToScreen = [&](glm::vec2 uv) -> ImVec2 {
        return ImVec2(
            canvasOrigin.x + uv.x * canvasW,
            canvasOrigin.y + (1.0f - uv.y) * canvasH
        );
    };

    // First pass: bright fill for each triangle
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            continue;

        ImVec2 p0 = uvToScreen(vertices[i0].TexCoords);
        ImVec2 p1 = uvToScreen(vertices[i1].TexCoords);
        ImVec2 p2 = uvToScreen(vertices[i2].TexCoords);

        ImVec2 triPts[3] = { p0, p1, p2 };
        dl->AddConvexPolyFilled(triPts, 3, IM_COL32(255, 255, 255, 100));
    }

    // --- 4. Grid overlay ---
    const int gridDiv = 4;
    for (int gi = 0; gi <= gridDiv; ++gi)
    {
        float t = static_cast<float>(gi) / static_cast<float>(gridDiv);
        float x = canvasOrigin.x + t * canvasW;
        float y = canvasOrigin.y + t * canvasH;
        dl->AddLine(ImVec2(x, canvasOrigin.y), ImVec2(x, canvasOrigin.y + canvasH),
                    IM_COL32(100, 100, 100, 180));
        dl->AddLine(ImVec2(canvasOrigin.x, y), ImVec2(canvasOrigin.x + canvasW, y),
                    IM_COL32(100, 100, 100, 180));
    }

    // --- 5. Build UV vertex groups (welded = same position), cached per entity ---
    if (sel != m_CachedEntity || m_UVGroupsDirty)
    {
        m_CachedUVGroups = BuildUVGroups(vertices);
        m_CachedEntity   = sel;
        m_UVGroupsDirty  = false;
    }
    auto& uvGroups = m_CachedUVGroups;

    // --- 6. Mouse interaction: find closest UV group and drag ---
    float mx = (mousePos.x - canvasOrigin.x) / canvasW;
    float my = 1.0f - (mousePos.y - canvasOrigin.y) / canvasH;

    if (isActive && isHovered && mouseDown)
    {
        // Acquire: first press on a vertex → lock the group
        if (!m_Dragging)
        {
            m_DraggedGroup = -1;
            for (int g = 0; g < static_cast<int>(uvGroups.size()); ++g)
            {
                int vi = uvGroups[g][0];
                float dx_px = (vertices[vi].TexCoords.x - mx) * canvasW;
                float dy_px = (vertices[vi].TexCoords.y - my) * canvasH;
                if (dx_px * dx_px + dy_px * dy_px < 15.0f * 15.0f)
                {
                    m_DraggedGroup = g;
                    m_Dragging = true;
                    break;
                }
            }
        }

        // Drag: move locked group by delta, regardless of cursor position
        if (m_Dragging && m_DraggedGroup >= 0)
        {
            float du = mouseDelta.x / canvasW;
            float dv = -mouseDelta.y / canvasH;

            if (du != 0.0f || dv != 0.0f)
            {
                std::vector<glm::vec2> editedUVs(vertices.size());
                for (size_t vi = 0; vi < vertices.size(); ++vi)
                    editedUVs[vi] = vertices[vi].TexCoords;

                for (int vi : uvGroups[m_DraggedGroup])
                {
                    editedUVs[vi].x = std::clamp(editedUVs[vi].x + du, 0.0f, 1.0f);
                    editedUVs[vi].y = std::clamp(editedUVs[vi].y + dv, 0.0f, 1.0f);
                }

                // Pack UVs for the submesh's unique vertices in sorted order (matches MeshLoader contract).
                const CHEngine::SubMesh& sm = gpuRec->subMeshes[kEditedSubmesh];
                std::vector<uint32_t> unique;
                unique.reserve(sm.indexCount);
                const uint32_t endIdx = sm.startIndex + sm.indexCount;
                for (uint32_t ii = sm.startIndex; ii < endIdx && ii < indices.size(); ++ii)
                {
                    const int64_t vidx = static_cast<int64_t>(indices[ii]) + sm.baseVertex;
                    if (vidx >= 0 && static_cast<size_t>(vidx) < vertices.size())
                        unique.push_back(static_cast<uint32_t>(vidx));
                }
                std::sort(unique.begin(), unique.end());
                unique.erase(std::unique(unique.begin(), unique.end()), unique.end());

                std::vector<glm::vec2> submeshUVs;
                submeshUVs.reserve(unique.size());
                for (uint32_t vi : unique) submeshUVs.push_back(editedUVs[vi]);

                meshLoader->UpdateVertexUVs(meshRef.Handle(), kEditedSubmesh, submeshUVs);
                m_UVGroupsDirty = true;
            }
        }
    }

    if (!mouseDown)
    {
        m_Dragging = false;
        m_DraggedGroup = -1;
    }

    // --- 7. Highlight: dragged group during drag, hover otherwise ---
    if (m_Dragging && m_DraggedGroup >= 0)
    {
        m_SelectedVertex = uvGroups[m_DraggedGroup][0];
    }
    else if (isHovered)
    {
        m_SelectedVertex = -1;
        for (int g = 0; g < static_cast<int>(uvGroups.size()); ++g)
        {
            int vi = uvGroups[g][0];
            float dx_px = (vertices[vi].TexCoords.x - mx) * canvasW;
            float dy_px = (vertices[vi].TexCoords.y - my) * canvasH;
            if (dx_px * dx_px + dy_px * dy_px < 15.0f * 15.0f)
            {
                m_SelectedVertex = vi;
                break;
            }
        }
    }
    else
    {
        m_SelectedVertex = -1;
    }

    // --- 8. Triangle wireframes ---
    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            continue;

        ImVec2 p0 = uvToScreen(vertices[i0].TexCoords);
        ImVec2 p1 = uvToScreen(vertices[i1].TexCoords);
        ImVec2 p2 = uvToScreen(vertices[i2].TexCoords);

        dl->AddTriangle(p0, p1, p2, IM_COL32(200, 200, 50, 220), 1.5f);
    }

    // --- 9. Vertex dots (one dot per group, all coincident = one dot) ---
    for (const auto& group : uvGroups)
    {
        int vi = group[0];
        ImVec2 pos = uvToScreen(vertices[vi].TexCoords);
        if (vi == m_SelectedVertex)
        {
            dl->AddCircleFilled(pos, 6.0f, IM_COL32(255, 255, 50, 255));
            dl->AddCircle(pos, 8.0f, IM_COL32(255, 255, 255, 200), 0, 2.0f);
        }
        else
        {
            dl->AddCircleFilled(pos, 4.0f, IM_COL32(255, 200, 50, 220));
        }
    }

    // --- 10. Info ---
    ImGui::SetCursorScreenPos(ImVec2(canvasOrigin.x, canvasOrigin.y + canvasH + 4));
    ImGui::Text("Verts: %zu  Tris: %zu  Groups: %zu%s",
                vertices.size(), indices.size() / 3, uvGroups.size(),
                hasTexture ? "  (texture)" : "");

    ImGui::End();
}

} // namespace Sandbox
