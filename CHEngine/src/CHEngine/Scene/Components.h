#pragma once
#include <string>
#include <tuple>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/nil_generator.hpp>
#include "CHEngine/Mesh/Mesh.h"
#include "Light.h"
#include "Transform.h"
#include "SceneCamera.h"
#include <Physics/PhysicsTypes.h>
#include <Physics/IPhysicsBody.h>
#include <Physics/IPhysicsShape.h>
#include <glm/glm.hpp>

// Only components
// TODO: Make check if components can be serialized
namespace CHEngine {

    using UUID = boost::uuids::uuid;
    class IPhysicsBody;
    class IPhysicsShape;

    struct IDComponent
    {
        UUID Value = boost::uuids::nil_uuid();
        IDComponent() = default;
        explicit IDComponent(const UUID& uuid)
            : Value(uuid)
        {
        }
    };

    struct TagComponent {
        std::string         Name;
    };

    // Render
    struct TransformComponent {
        Transform ObjectTransform;
    };

    struct MeshComponent {
        std::vector<Mesh> Meshes;
        std::string       SourcePath;  // original file path for serialization

        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
        MeshComponent& operator=(const MeshComponent&) = default;
        MeshComponent(MeshComponent&&) = default;
        MeshComponent& operator=(MeshComponent&&) = default;
    };

    struct ColorComponent {
        glm::vec4 Color = { 0.8f, 0.8f, 0.8f, 1.0f };
    };

    struct VisibilityComponent {
        bool Visible = true;
    };

    struct LightComponent {
        Light LightData;
    };

    struct CameraComponent
    {
        SceneCamera Camera;
        bool FixedAspectRatio = false;
        bool Primary = true;
    };

    // Physics

    struct RigidBody3DComponent
    {
        PhysicsRigidBodyDesc BodyDesc{};
        PhysicsColliderShapeDesc ShapeDesc{};
        RigidBodySyncMode SyncMode = RigidBodySyncMode::Auto;

        IPhysicsBody* Body = nullptr;
        IPhysicsShape* Shape = nullptr;

        bool SynchronisedTransform = true;
    };

    struct LifetimeComponent
    {
        float RemainingSeconds = 0.0f;
        bool DestroyOnExpire = true;
    };

    struct ScriptEntry
    {
        std::string Path;
        bool        Enabled = true;
    };

    struct ScriptComponent
    {
        std::vector<ScriptEntry> Scripts;
    };

    // =========================================================================
    // UI Components
    // =========================================================================

    // UICanvasComponent — root container that defines the UI space.
    // Screen Space Overlay: drawn on top of everything, ignores camera.
    // World Space: the Canvas is a quad in 3D space.
    struct UICanvasComponent
    {
        enum class RenderMode : uint8_t { ScreenSpaceOverlay = 0, WorldSpace = 1 };
        RenderMode Mode      = RenderMode::ScreenSpaceOverlay;
        int        SortOrder = 0;   // higher = drawn on top
    };

    // UIRectTransform — 2D layout, similar to Unity's RectTransform.
    // Position is in pixels, anchored to a point on screen/parent.
    // Anchor (0,0) = top-left, (1,1) = bottom-right, (0.5,0.5) = center.
    struct UIRectTransformComponent
    {
        glm::vec2 AnchorMin  = { 0.5f, 0.5f }; // normalized, top-left = (0,0)
        glm::vec2 AnchorMax  = { 0.5f, 0.5f }; // for stretch: set different from Min
        glm::vec2 Position   = { 0.0f, 0.0f }; // pixel offset from anchor
        glm::vec2 Size       = { 160.0f, 40.0f }; // pixels
        glm::vec2 Pivot      = { 0.5f, 0.5f }; // point inside element used as origin
        float     Rotation   = 0.0f;            // degrees
        float     Alpha      = 1.0f;            // 0=transparent, 1=opaque
        int       ZOrder     = 0;               // depth within canvas
    };

    // UIImageComponent — displays a flat colour or a texture.
    struct UIImageComponent
    {
        glm::vec4   Color        = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::string TexturePath;   // empty = solid colour
        bool        PreserveAspect = true;
        bool        SlicedBorder   = false; // 9-slice (future)
    };

    // UITextComponent — renders text with a custom font.
    struct UITextComponent
    {
        std::string Text     = "Text";
        std::string FontPath;          // absolute path to TTF/OTF; empty = default
        float       FontSize = 16.0f;
        glm::vec4   Color    = { 1.0f, 1.0f, 1.0f, 1.0f };
        enum class HAlign : uint8_t { Left = 0, Center = 1, Right = 2 };
        enum class VAlign : uint8_t { Top  = 0, Middle = 1, Bottom = 2 };
        HAlign HorizontalAlign = HAlign::Center;
        VAlign VerticalAlign   = VAlign::Middle;
        bool   Bold            = false;
        bool   Italic          = false;
        bool   WordWrap        = true;
    };

    // UIPanelComponent — rounded background rectangle with optional border.
    struct UIPanelComponent
    {
        glm::vec4 Color        = { 0.10f, 0.10f, 0.12f, 0.90f };
        glm::vec4 BorderColor  = { 0.30f, 0.30f, 0.35f, 1.00f };
        float     BorderWidth  = 0.0f;
        float     CornerRadius = 6.0f;
    };

    // UIButtonComponent — clickable element with hover/pressed visual feedback.
    // OnClick is a Lua function name called on the entity's script.
    struct UIButtonComponent
    {
        glm::vec4   NormalColor  = { 1.00f, 1.00f, 1.00f, 1.00f };
        glm::vec4   HoverColor   = { 0.85f, 0.90f, 1.00f, 1.00f };
        glm::vec4   PressedColor = { 0.65f, 0.75f, 1.00f, 1.00f };
        glm::vec4   DisabledColor= { 0.50f, 0.50f, 0.50f, 0.60f };
        std::string OnClick;        // Lua function name ("OnButtonClick")
        bool        Interactable = true;
        float       CornerRadius = 6.0f;
    };

    // UISliderComponent — horizontal slider.
    struct UISliderComponent
    {
        float     Value           = 0.5f;
        float     Min             = 0.0f;
        float     Max             = 1.0f;
        glm::vec4 BackgroundColor = { 0.20f, 0.20f, 0.22f, 1.00f };
        glm::vec4 FillColor       = { 0.04f, 0.52f, 1.00f, 1.00f };
        glm::vec4 HandleColor     = { 1.00f, 1.00f, 1.00f, 1.00f };
        float     HandleSize      = 16.0f;
        bool      Interactable    = true;
        std::string OnChange;         // Lua function name
    };

	template<typename... Components>
	struct ComponentGroup
	{
		using types = std::tuple<Components...>;
	};

    // TODO: Copying between scenes could be better
    using CopyableSceneComponents = ComponentGroup<
        TransformComponent,
        ColorComponent,
        VisibilityComponent,
        LightComponent,
        CameraComponent,
        LifetimeComponent,
        ScriptComponent,
        RigidBody3DComponent,
        MeshComponent,
        UICanvasComponent,
        UIRectTransformComponent,
        UIImageComponent,
        UITextComponent,
        UIPanelComponent,
        UIButtonComponent,
        UISliderComponent>;
}
