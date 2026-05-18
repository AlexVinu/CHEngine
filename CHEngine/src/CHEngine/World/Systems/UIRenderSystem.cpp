#include "chepch.h"
#include "UIRenderSystem.h"

#include "CHEngine/Application.h"
#include "CHEngine/World/World.h"
#include "CHEngine/Scene/Scene.h"
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/UI/UIRendererBackend.h"
#include "CHEngine/UI/Font/FontAtlasLoader.h"
#include "CHEngine/Utils/AppPaths.h"

#include <Render/UniformBlocks.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstring>

namespace CHEngine {

// ─── ResolveRect ─────────────────────────────────────────────────────────────
//
// Layout a child UIRectTransform inside a parent (canvas or another rect) whose
// local coordinate system is (0,0)..(parentW,parentH) in pixels, Y-down.
UIRenderSystem::UIRect UIRenderSystem::ResolveRect(
    const UIRectTransformComponent& rt, float parentW, float parentH)
{
    float x = rt.AnchorMin.x * parentW - rt.Pivot.x * rt.Size;
    float y = rt.AnchorMin.y * parentH - rt.Pivot.y * rt.Size;
    return { x, y, rt.Size, rt.Size };
}

// ─── Font caching helper ─────────────────────────────────────────────────────

namespace {

using FontCache = std::unordered_map<std::string, FontAtlasHandle>;

FontAtlasHandle GetFontFromCache(UIRendererBackend& backend,
                                 FontCache& cache,
                                 const std::string& path, float size)
{
    if (path.empty()) return FontAtlasHandle{};
    std::string key = path + ":" + std::to_string(static_cast<int>(size));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    std::filesystem::path fsPath(path);
    if (!fsPath.is_absolute())
        fsPath = AppPaths::ExecutableDir() / fsPath;
    FontAtlasHandle h = backend.GetAtlasLoader().Load(fsPath, size);
    cache[key] = h;
    return h;
}

} // namespace

// ─── DrawElement ─────────────────────────────────────────────────────────────

void UIRenderSystem::DrawElement(UIRendererBackend* backend,
                                 Entity* entity,
                                 const UIRect& rect,
                                 float canvasAlpha)
{
    static thread_local FontCache fontCache; // small per-frame churn ok

    const auto& rt = entity->GetComponent<UIRectTransformComponent>();
    float alpha = canvasAlpha * rt.Alpha;

    auto entityColor = [&](glm::vec4 def) -> glm::vec4 {
        return entity->HasComponent<ColorComponent>()
            ? entity->GetComponent<ColorComponent>().Color
            : def;
    };

    if (entity->HasComponent<UIPanelComponent>())
    {
        const auto& c = entity->GetComponent<UIPanelComponent>();
        glm::vec4 raw  = entityColor({ 0.10f, 0.10f, 0.12f, 0.90f });
        glm::vec4 col  = { raw.r, raw.g, raw.b, raw.a * alpha };
        glm::vec4 bcol = { c.BorderColor.r, c.BorderColor.g,
                           c.BorderColor.b, c.BorderColor.a * alpha };
        backend->DrawQuad(rect.x, rect.y, c.Width, rect.h,
                          col, c.CornerRadius, c.BorderWidth, bcol);
    }

    if (entity->HasComponent<UIImageComponent>())
    {
        const auto& c = entity->GetComponent<UIImageComponent>();
        glm::vec4 raw  = entityColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        glm::vec4 tint = { raw.r, raw.g, raw.b, raw.a * alpha };
        backend->DrawTexturedQuad(rect.x, rect.y, c.Width, rect.h,
                                  TextureHandle{}, tint, 0.f);
    }

    if (entity->HasComponent<UIButtonComponent>())
    {
        const auto& btn = entity->GetComponent<UIButtonComponent>();
        glm::vec4 col = btn.Interactable ? btn.NormalColor : btn.DisabledColor;
        col.a *= alpha;
        backend->DrawQuad(rect.x, rect.y, btn.Width, rect.h, col, btn.CornerRadius);
    }

    if (entity->HasComponent<UISliderComponent>())
    {
        const auto& c    = entity->GetComponent<UISliderComponent>();
        float        norm = (c.Max > c.Min) ? (c.Value - c.Min) / (c.Max - c.Min) : 0.f;
        norm = std::clamp(norm, 0.f, 1.f);

        float trackY = rect.y + rect.h * 0.35f;
        float trackH = rect.h * 0.30f;
        glm::vec4 bg = { c.BackgroundColor.r, c.BackgroundColor.g,
                         c.BackgroundColor.b, c.BackgroundColor.a * alpha };
        backend->DrawQuad(rect.x, trackY, c.Width, trackH, bg, 4.f);

        if (norm > 0.f)
        {
            glm::vec4 fill = { c.FillColor.r, c.FillColor.g,
                               c.FillColor.b, c.FillColor.a * alpha };
            backend->DrawQuad(rect.x, trackY, c.Width * norm, trackH, fill, 4.f);
        }

        float hx = rect.x + c.Width * norm - c.HandleSize * 0.5f;
        float hy = rect.y + rect.h * 0.5f - c.HandleSize * 0.5f;
        glm::vec4 hcol = { c.HandleColor.r, c.HandleColor.g,
                           c.HandleColor.b, c.HandleColor.a * alpha };
        backend->DrawQuad(hx, hy, c.HandleSize, c.HandleSize, hcol,
                          c.HandleSize * 0.5f);
    }

    if (entity->HasComponent<UITextComponent>())
    {
        const auto& c = entity->GetComponent<UITextComponent>();
        if (!c.Text.empty())
        {
            const float fontSize = rect.h;
            FontAtlasHandle fh = GetFontFromCache(*backend, fontCache,
                                                  c.FontPath, fontSize);
            glm::vec4 col = { c.Color.r, c.Color.g, c.Color.b, c.Color.a * alpha };
            backend->DrawUIText(c.Text, rect.x, rect.y, rect.w, rect.h,
                                fh, fontSize, col);
        }
    }
}

// ─── Run ─────────────────────────────────────────────────────────────────────

void UIRenderSystem::Run(World& world, DeferredOps& /*deferred*/, Timestep /*ts*/)
{
    auto scene = world.GetSceneRef();
    if (!scene) return;

    UIRendererBackend* backend = Application::Get().NativeUI();
    if (!backend) return;

    const uint32_t sw = Application::Get().Render().GetViewportWidth();
    const uint32_t sh = Application::Get().Render().GetViewportHeight();
    if (sw == 0 || sh == 0) return;

    backend->BeginFrame(sw, sh);

    // Step 1: build UUID → EntityHandle index of canvases.
    struct CanvasInfo
    {
        EntityHandle handle;
        bool         isOverlay;
        int          sortOrder;
    };
    std::unordered_map<UUID, CanvasInfo, boost::hash<UUID>> canvasIndex;

    scene->ForEach<UIOverlayCanvasComponent>(
        [&](EntityHandle h, const UUID& uuid, UIOverlayCanvasComponent& c)
        {
            auto* e = scene->TryGetEntity(h);
            if (e && e->HasComponent<VisibilityComponent>() &&
                !e->GetComponent<VisibilityComponent>().Visible) return;
            canvasIndex[uuid] = { h, /*overlay*/ true, c.SortOrder };
        });

    scene->ForEach<UIWorldCanvasComponent>(
        [&](EntityHandle h, const UUID& uuid, UIWorldCanvasComponent& /*c*/)
        {
            auto* e = scene->TryGetEntity(h);
            if (e && e->HasComponent<VisibilityComponent>() &&
                !e->GetComponent<VisibilityComponent>().Visible) return;
            canvasIndex[uuid] = { h, /*overlay*/ false, 0 };
        });

    if (canvasIndex.empty()) return;

    // Step 2: group elements by CanvasRef.
    std::unordered_map<UUID, std::vector<EntityHandle>, boost::hash<UUID>>
        elementsByCanvas;

    scene->ForEach<UIRectTransformComponent>(
        [&](EntityHandle h, const UUID&, UIRectTransformComponent& rt)
        {
            auto it = canvasIndex.find(rt.CanvasRef);
            if (it == canvasIndex.end()) return;
            elementsByCanvas[rt.CanvasRef].push_back(h);
        });

    // Step 3: separate overlay & world canvas lists, sort overlay by SortOrder.
    std::vector<UUID> overlayList;
    std::vector<UUID> worldList;
    overlayList.reserve(canvasIndex.size());
    worldList.reserve(canvasIndex.size());
    for (const auto& [uuid, info] : canvasIndex)
        (info.isOverlay ? overlayList : worldList).push_back(uuid);

    std::sort(overlayList.begin(), overlayList.end(),
        [&](const UUID& a, const UUID& b)
        { return canvasIndex[a].sortOrder < canvasIndex[b].sortOrder; });

    auto SortElements = [&](std::vector<EntityHandle>& list)
    {
        std::sort(list.begin(), list.end(),
            [&](EntityHandle a, EntityHandle b)
            {
                auto* ea = scene->TryGetEntity(a);
                auto* eb = scene->TryGetEntity(b);
                int za = ea ? ea->GetComponent<UIRectTransformComponent>().ZOrder : 0;
                int zb = eb ? eb->GetComponent<UIRectTransformComponent>().ZOrder : 0;
                return za < zb;
            });
    };

    // ── Overlay pass ──────────────────────────────────────────────────────────
    if (!overlayList.empty())
    {
        backend->BeginOverlay();
        for (const UUID& uuid : overlayList)
        {
            auto* canvasEnt = scene->TryGetEntity(canvasIndex[uuid].handle);
            if (!canvasEnt) continue;
            const auto& c = canvasEnt->GetComponent<UIOverlayCanvasComponent>();

            // Size нормализован в [0..1] — конвертим в пиксели один раз.
            const float canvasW = c.Size.x * static_cast<float>(sw);
            const float canvasH = c.Size.y * static_cast<float>(sh);

            // Resolve canvas rect on screen (Unity-style anchor + pivot).
            float canvasX = c.AnchorMin.x * static_cast<float>(sw)
                          + c.Position.x - c.Pivot.x * canvasW;
            float canvasY = c.AnchorMin.y * static_cast<float>(sh)
                          + c.Position.y - c.Pivot.y * canvasH;

            // Draw canvas background using ColorComponent on the canvas entity.
            glm::vec4 bg = { 0.0f, 0.0f, 0.0f, 0.0f };
            if (canvasEnt->HasComponent<ColorComponent>())
            {
                const auto& cc = canvasEnt->GetComponent<ColorComponent>();
                bg = { cc.Color.r, cc.Color.g, cc.Color.b, cc.Color.a * c.Alpha };
            }
            if (bg.a > 0.0f)
                backend->DrawQuad(canvasX, canvasY, canvasW, canvasH, bg);

            // Children: positions are in canvas-local pixels, but the overlay
            // submission uses screen ortho, so offset by (canvasX, canvasY).
            auto eit = elementsByCanvas.find(uuid);
            if (eit == elementsByCanvas.end()) continue;
            SortElements(eit->second);

            backend->SetClipRect(canvasX, canvasY, canvasW, canvasH);
            for (EntityHandle eh : eit->second)
            {
                auto* e = scene->TryGetEntity(eh);
                if (!e) continue;
                if (e->HasComponent<VisibilityComponent>() &&
                    !e->GetComponent<VisibilityComponent>().Visible)
                    continue;
                const auto& rt = e->GetComponent<UIRectTransformComponent>();
                UIRect r = ResolveRect(rt, canvasW, canvasH);
                r.x += canvasX;
                r.y += canvasY;
                DrawElement(backend, e, r, c.Alpha);
            }
            backend->ClearClipRect();
        }
        backend->EndCanvas();
    }

    // ── World canvases ────────────────────────────────────────────────────────
    if (!worldList.empty())
    {
        const auto& sceneCam = Application::Get().Render().GetSceneCamera();
        glm::mat4 cameraVP;
        std::memcpy(glm::value_ptr(cameraVP),
                    sceneCam.ViewProjection, sizeof(cameraVP));

        for (const UUID& uuid : worldList)
        {
            auto* canvasEnt = scene->TryGetEntity(canvasIndex[uuid].handle);
            if (!canvasEnt) continue;
            const auto& c = canvasEnt->GetComponent<UIWorldCanvasComponent>();
            if (!canvasEnt->HasComponent<TransformComponent>()) continue;

            glm::mat4 model =
                canvasEnt->GetComponent<TransformComponent>().ObjectTransform.GetMatrix();

            // Virtual pixel resolution of the canvas: 100 px per meter
            // so that UI elements specified in pixels stay readable in world.
            constexpr float kPixPerMeter = 100.0f;
            glm::vec2 sizePx = c.Size * kPixPerMeter;

            // pixelToLocal: maps canvas-pixel (0..Wpx, 0..Hpx, Y-down)
            // → canvas-local meters in XY plane (X right, Y up), Z=0, centered at origin.
            glm::mat4 pixelToLocal =
                glm::translate(glm::mat4(1.0f), glm::vec3(-c.Size.x * 0.5f,  c.Size.y * 0.5f, 0.0f)) *
                glm::scale    (glm::mat4(1.0f), glm::vec3(c.Size.x / sizePx.x,
                                                          -c.Size.y / sizePx.y, 1.0f));

            glm::mat4 viewProj = cameraVP * model * pixelToLocal;

            backend->BeginCanvas(viewProj, /*depthTest*/ true);

            // Canvas background quad using ColorComponent on the canvas entity.
            glm::vec4 bg = { 0.0f, 0.0f, 0.0f, 0.0f };
            if (canvasEnt->HasComponent<ColorComponent>())
            {
                const auto& cc = canvasEnt->GetComponent<ColorComponent>();
                bg = { cc.Color.r, cc.Color.g, cc.Color.b, cc.Color.a * c.Alpha };
            }
            if (bg.a > 0.0f)
                backend->DrawQuad(0.0f, 0.0f, sizePx.x, sizePx.y, bg);

            auto eit = elementsByCanvas.find(uuid);
            if (eit != elementsByCanvas.end())
            {
                SortElements(eit->second);
                for (EntityHandle eh : eit->second)
                {
                    auto* e = scene->TryGetEntity(eh);
                    if (!e) continue;
                    if (e->HasComponent<VisibilityComponent>() &&
                        !e->GetComponent<VisibilityComponent>().Visible)
                        continue;
                    const auto& rt = e->GetComponent<UIRectTransformComponent>();
                    UIRect r = ResolveRect(rt, sizePx.x, sizePx.y);
                    DrawElement(backend, e, r, c.Alpha);
                }
            }

            backend->EndCanvas();
        }
    }

    backend->Flush();
}

} // namespace CHEngine
