#include "chepch.h"
#include "UIInputSystem.h"

#include "CHEngine/Application.h"
#include "CHEngine/Input/Input.h"
#include <Input/KeyCodes.h>
#include "CHEngine/Scene/Entity.h"
#include "CHEngine/Scene/Components.h"
#include "CHEngine/World/World.h"
#include "CHEngine/World/Systems/LuaScriptSystem.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace CHEngine {

namespace {

struct UIRect { float x, y, w, h; };

bool PointInRect(float px, float py, const UIRect& r)
{
    return px >= r.x && py >= r.y && px <= r.x + r.w && py <= r.y + r.h;
}

UIRect ResolveElementRect(const UIRectTransformComponent& rt,
                          float parentW, float parentH)
{
    // Совпадает с UIRenderSystem::ResolveRect.
    float x = rt.AnchorMin.x * parentW - rt.Pivot.x * rt.Size;
    float y = rt.AnchorMin.y * parentH - rt.Pivot.y * rt.Size;
    return { x, y, rt.Size, rt.Size };
}

bool EntityVisible(const Entity* e)
{
    if (!e) return false;
    if (e->HasComponent<VisibilityComponent>() &&
        !e->GetComponent<VisibilityComponent>().Visible)
        return false;
    return true;
}

} // namespace

UIInputSystem::UIInputSystem(uint8_t priority)
    : ISystem(SystemPhase::Simulation, priority)
{}

void UIInputSystem::Run(World& world, DeferredOps& deferred, Timestep /*dt*/)
{
    auto scene = world.GetSceneRef();
    if (!scene) return;

    const uint32_t sw = Application::Get().Render().GetViewportWidth();
    const uint32_t sh = Application::Get().Render().GetViewportHeight();
    if (sw == 0 || sh == 0) return;

    const float mx = Input::GetMouseX();
    const float my = Input::GetMouseY();
    const bool  mDown     = Input::IsMouseButtonDown(MouseButton::Left);
    const bool  mClicked  = Input::IsMouseButtonPressed(MouseButton::Left);
    const bool  mReleased = Input::IsMouseButtonReleased(MouseButton::Left);

    // ── Сбор канвасов и их экранных rect ─────────────────────────────────────
    struct CanvasInfo
    {
        EntityHandle handle;
        UIRect       screenRect;   // в пикселях viewport
        UIRect       localBox;     // 0,0,canvasW,canvasH
        int          sortOrder;
    };

    std::vector<CanvasInfo> overlayCanvases;
    std::unordered_map<UUID, size_t, std::hash<UUID>> canvasByUUID;

    scene->ForEach<UIOverlayCanvasComponent>(
        [&](EntityHandle h, const UUID& uuid, UIOverlayCanvasComponent& c)
        {
            auto* e = scene->TryGetEntity(h);
            if (!EntityVisible(e)) return;

            const float canvasW = c.Size.x * static_cast<float>(sw);
            const float canvasH = c.Size.y * static_cast<float>(sh);
            const float canvasX = c.AnchorMin.x * static_cast<float>(sw)
                                + c.Position.x - c.Pivot.x * canvasW;
            const float canvasY = c.AnchorMin.y * static_cast<float>(sh)
                                + c.Position.y - c.Pivot.y * canvasH;

            CanvasInfo info{ h, { canvasX, canvasY, canvasW, canvasH },
                             { 0.f, 0.f, canvasW, canvasH }, c.SortOrder };
            canvasByUUID[uuid] = overlayCanvases.size();
            overlayCanvases.push_back(info);
        });

    // ── Сбор элементов по канвасу ────────────────────────────────────────────
    std::unordered_map<UUID, std::vector<EntityHandle>, std::hash<UUID>>
        elementsByCanvas;

    scene->ForEach<UIRectTransformComponent, ParentNodeComponent>(
        [&](EntityHandle h, const UUID&, UIRectTransformComponent& rt, ParentNodeComponent& parent)
        {
            if (!canvasByUUID.count(parent.Value)) return;
            elementsByCanvas[parent.Value].push_back(h);
        });

    // ── Сначала обновляем активный слайдер (drag в процессе) ─────────────────
    if (m_ActiveSlider.IsValid())
    {
        auto* e = scene->TryGetEntity(m_ActiveSlider);
        if (!e || !e->HasComponent<UISliderComponent>() ||
            !e->HasComponent<UIRectTransformComponent>())
        {
            m_ActiveSlider = {};
        }
        else
        {
            const auto& rt = e->GetComponent<UIRectTransformComponent>();
            const auto& parent = e->GetComponent<ParentNodeComponent>();
            auto cit = canvasByUUID.find(parent.Value);
            if (cit == canvasByUUID.end())
            {
                m_ActiveSlider = {};
            }
            else
            {
                const auto& canvas = overlayCanvases[cit->second];
                UIRect local = ResolveElementRect(rt, canvas.localBox.w, canvas.localBox.h);
                UIRect r{ local.x + canvas.screenRect.x,
                          local.y + canvas.screenRect.y, local.w, local.h };

                const auto& slider = e->GetComponent<UISliderComponent>();
                const float sliderW = slider.Width;

                if (mDown)
                {
                    if (slider.Interactable && sliderW > 0.f && slider.Max > slider.Min)
                    {
                        float t = (mx - r.x) / sliderW;
                        t = std::clamp(t, 0.f, 1.f);
                        float newValue = slider.Min + t * (slider.Max - slider.Min);
                        if (newValue != slider.Value)
                        {
                            EntityHandle eh = m_ActiveSlider;
                            e->PatchComponent<UISliderComponent>(
                                [newValue](UISliderComponent& s){ s.Value = newValue; });
                            LuaCallEntityFunction(world, deferred, eh, "OnChange", newValue);
                        }
                    }
                }
                else if (mReleased)
                {
                    const float finalValue = slider.Value;
                    EntityHandle eh = m_ActiveSlider;
                    m_ActiveSlider = {};
                    LuaCallEntityFunction(world, deferred, eh, "OnRelease", finalValue);
                }
                else
                {
                    // мышь не нажата и не отпускалась этим кадром — drag прекращён без события
                    m_ActiveSlider = {};
                }
            }
        }
    }

    // ── Hit-test для новых событий (только если нет активного drag) ──────────
    if (!m_ActiveSlider.IsValid() && (mClicked || mReleased))
    {
        // Сортируем канвасы по SortOrder (сверху = выше)
        std::vector<size_t> canvasOrder(overlayCanvases.size());
        for (size_t i = 0; i < canvasOrder.size(); ++i) canvasOrder[i] = i;
        std::sort(canvasOrder.begin(), canvasOrder.end(),
            [&](size_t a, size_t b)
            { return overlayCanvases[a].sortOrder > overlayCanvases[b].sortOrder; });

        bool consumed = false;

        for (size_t ci : canvasOrder)
        {
            if (consumed) break;
            const auto& canvas = overlayCanvases[ci];
            if (!PointInRect(mx, my, canvas.screenRect)) continue;

            // Найти UUID по индексу
            UUID canvasUuid{};
            for (auto& kv : canvasByUUID)
                if (kv.second == ci) { canvasUuid = kv.first; break; }

            auto eit = elementsByCanvas.find(canvasUuid);
            if (eit == elementsByCanvas.end()) continue;

            // Сортируем элементы по ZOrder (выше = сверху)
            std::vector<EntityHandle> sorted = eit->second;
            std::sort(sorted.begin(), sorted.end(),
                [&](EntityHandle a, EntityHandle b)
                {
                    auto* ea = scene->TryGetEntity(a);
                    auto* eb = scene->TryGetEntity(b);
                    int za = ea ? ea->GetComponent<UIRectTransformComponent>().ZOrder : 0;
                    int zb = eb ? eb->GetComponent<UIRectTransformComponent>().ZOrder : 0;
                    return za > zb;
                });

            for (EntityHandle eh : sorted)
            {
                auto* e = scene->TryGetEntity(eh);
                if (!EntityVisible(e)) continue;
                const auto& rt = e->GetComponent<UIRectTransformComponent>();
                UIRect local = ResolveElementRect(rt, canvas.localBox.w, canvas.localBox.h);
                UIRect r{ local.x + canvas.screenRect.x,
                          local.y + canvas.screenRect.y, local.w, local.h };

                // Button
                if (e->HasComponent<UIButtonComponent>())
                {
                    const auto& btn = e->GetComponent<UIButtonComponent>();
                    UIRect br{ r.x, r.y, btn.Width, r.h };
                    if (!PointInRect(mx, my, br)) continue;
                    if (!btn.Interactable) { consumed = true; break; }

                    if (mClicked)
                    {
                        m_PressedButton = eh;
                        consumed = true;
                        break;
                    }
                    if (mReleased)
                    {
                        if (m_PressedButton == eh)
                            LuaCallEntityFunction(world, deferred, eh, "OnClick");
                        m_PressedButton = {};
                        consumed = true;
                        break;
                    }
                }
                // Slider
                else if (e->HasComponent<UISliderComponent>())
                {
                    const auto& sl = e->GetComponent<UISliderComponent>();
                    UIRect sr{ r.x, r.y, sl.Width, r.h };
                    if (!PointInRect(mx, my, sr)) continue;
                    if (!sl.Interactable) { consumed = true; break; }

                    if (mClicked)
                    {
                        m_ActiveSlider = eh;
                        // сразу выставляем значение по клику
                        if (sl.Width > 0.f && sl.Max > sl.Min)
                        {
                            float t = std::clamp((mx - r.x) / sl.Width, 0.f, 1.f);
                            float newValue = sl.Min + t * (sl.Max - sl.Min);
                            if (newValue != sl.Value)
                            {
                                e->PatchComponent<UISliderComponent>(
                                    [newValue](UISliderComponent& s){ s.Value = newValue; });
                                LuaCallEntityFunction(world, deferred, eh, "OnChange", newValue);
                            }
                        }
                        consumed = true;
                        break;
                    }
                }
            }
        }

        // Если кликнули вне любой кнопки — сбросить m_PressedButton по mReleased
        if (mReleased && !consumed)
            m_PressedButton = {};
    }
}


void UIInputSystem::OnBegin(World& world, DeferredOps& deferred_ops)
{

}


void UIInputSystem::OnEnd(World& world, DeferredOps& deferred_ops)
{

}

} // namespace CHEngine
