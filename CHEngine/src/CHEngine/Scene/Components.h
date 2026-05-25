#pragma once
#include <string>
#include <tuple>
#include <vector>
#include <limits>
#include "UUID.h"
#include "CHEngine/Mesh/MeshRef.h"
#include "Light.h"
#include "Transform.h"
#include "CHEngine/Camera/PerspectiveCamera.h"
#include "CHEngine/Camera/OrthographicCamera.h"
#include <Physics/PhysicsTypes.h>
#include <Physics/IPhysicsShape.h>  
#include <Physics/Handles.h>
#include <glm/glm.hpp>


// NOTE: Components must be POD structures
// After making new components you have to register them in metaserializer, component meta and add AllComponents below this file
namespace CHEngine {

    using StringID = uint32_t;
    using ScriptsID = uint32_t;

    template<typename T>
    inline constexpr T INVALID_ID = std::numeric_limits<T>::max();

	struct ScriptEntry
	{
		std::string Path;
		bool        Enabled = true;
	};

    struct IDComponent
    {
        UUID Value{};
        IDComponent() = default;
        explicit IDComponent(const UUID& uuid)
            : Value(uuid)
        {
        }
    };

    struct TagComponent {
        StringID Name = INVALID_ID<StringID>;
    };

    struct ParentNodeComponent
    {
        UUID Value{};
    };

    // Render
    struct TransformComponent {
        Transform ObjectTransform;
        bool Dirty = true;

        void MarkDirty() { Dirty = true; }
    };

    struct MeshComponent {
        MeshRef     Mesh;
        StringID SourcePath = INVALID_ID<StringID>;  // original file path for serialization
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
        CameraVariant Camera = PerspectiveCamera{};
        bool FixedAspectRatio = false;
        bool Primary = true;
        bool IsActive = false;
    };

    // Physics

    struct RigidBody3DComponent
    {
        PhysicsRigidBodyDesc BodyDesc{};
        PhysicsColliderShapeDesc ShapeDesc{};
        RigidBodySyncMode SyncMode = RigidBodySyncMode::Auto;

        PhysBodyHandle  Body{};
        PhysShapeHandle Shape{};

        bool SynchronisedTransform = true;
    };

    struct LifetimeComponent
    {
        float RemainingSeconds = 0.0f;
        bool DestroyOnExpire = true;
    };

    struct ScriptComponent
    {
        ScriptsID Scripts = INVALID_ID<ScriptsID>;
    };

    // =========================================================================
    // UI Components
    // =========================================================================

    // UIOverlayCanvasComponent — root container для screen-space UI.
    struct UIOverlayCanvasComponent
    {
        glm::vec2 AnchorMin = { 0.5f, 0.5f };
        glm::vec2 AnchorMax = { 0.5f, 0.5f };
        glm::vec2 Position  = { 0.0f, 0.0f };
        glm::vec2 Size      = { 1.0f, 1.0f }; // нормализованный размер (доля от экрана 0..1)
        glm::vec2 Pivot     = { 0.5f, 0.5f };
        int       SortOrder = 0;              // выше = поверх
        float     Alpha     = 1.0f;
    };

    // UIWorldCanvasComponent — root container для UI в мире.
    struct UIWorldCanvasComponent
    {
        glm::vec2 Size       = { 2.0f, 1.0f };
        float     Alpha      = 1.0f;
        bool      DoubleSided = true;
    };

    // На самой канвас-entity этот компонент НЕ присутствует.
    struct UIRectTransformComponent
    {
        glm::vec2 AnchorMin  = { 0.5f, 0.5f }; // нормализованный якорь внутри канваса
        glm::vec2 AnchorMax  = { 0.5f, 0.5f };
        float     Size       = 40.0f;  // высота / размер шрифта в пикселях
        glm::vec2 Pivot      = { 0.5f, 0.5f };
        float     Alpha      = 1.0f;
        int       ZOrder     = 0;
    };

    // UIImageComponent — displays a flat colour or a texture.
    struct UIImageComponent
    {
        float       Width          = 160.0f;
        StringID TexturePath = INVALID_ID<StringID>;  // empty = solid colour
        bool        PreserveAspect = true;
        bool        SlicedBorder   = false; // 9-slice (future)
    };

    // UITextComponent — renders text with a custom font.
    struct UITextComponent
    {
        StringID Text     = INVALID_ID<StringID>;
        StringID FontPath = INVALID_ID<StringID>;  // absolute path to TTF/OTF; empty = default
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
        float     Width        = 160.0f;
        glm::vec4 BorderColor  = { 0.30f, 0.30f, 0.35f, 1.00f };
        float     BorderWidth  = 0.0f;
        float     CornerRadius = 6.0f;
    };

    // UIButtonComponent — clickable element with hover/pressed visual feedback.
    // Скрипт на entity вызывается через convention: function OnClick(entity).
    struct UIButtonComponent
    {
        float       Width        = 160.0f;
        glm::vec4   NormalColor  = { 1.00f, 1.00f, 1.00f, 1.00f };
        glm::vec4   HoverColor   = { 0.85f, 0.90f, 1.00f, 1.00f };
        glm::vec4   PressedColor = { 0.65f, 0.75f, 1.00f, 1.00f };
        glm::vec4   DisabledColor= { 0.50f, 0.50f, 0.50f, 0.60f };
        bool        Interactable = true;
        float       CornerRadius = 6.0f;
    };

    // UISliderComponent — horizontal slider.
    struct UISliderComponent
    {
        float     Width           = 200.0f;
        float     Value           = 0.5f;
        float     Min             = 0.0f;
        float     Max             = 1.0f;
        glm::vec4 BackgroundColor = { 0.20f, 0.20f, 0.22f, 1.00f };
        glm::vec4 FillColor       = { 0.04f, 0.52f, 1.00f, 1.00f };
        glm::vec4 HandleColor     = { 1.00f, 1.00f, 1.00f, 1.00f };
        float     HandleSize      = 16.0f;
        bool      Interactable    = true;
    };

	template<typename... Components>
	struct ComponentGroup
	{
		using types = std::tuple<Components...>;
	};


	using AllComponents = ComponentGroup<
		IDComponent,
		TagComponent,
		ParentNodeComponent,
		TransformComponent,
		MeshComponent,
		ColorComponent,
		VisibilityComponent,
		LightComponent,
		CameraComponent,
		RigidBody3DComponent,
		LifetimeComponent,
		ScriptComponent,
		UIOverlayCanvasComponent,
		UIWorldCanvasComponent,
		UIRectTransformComponent,
		UIImageComponent,
		UITextComponent,
		UIPanelComponent,
		UIButtonComponent,
		UISliderComponent>;
}
