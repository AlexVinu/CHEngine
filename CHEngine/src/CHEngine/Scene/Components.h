#pragma once
#include <string>
#include <tuple>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/container_hash/hash.hpp>
#include <spdlog/fmt/fmt.h>
#include "CHEngine/Mesh/Mesh.h"
#include "Light.h"
#include "Transform.h"
#include "CHEngine/Camera/PerspectiveCamera.h"
#include "CHEngine/Camera/OrthographicCamera.h"
#include <Physics/PhysicsTypes.h>
#include <Physics/IPhysicsBody.h>
#include <Physics/IPhysicsShape.h>
#include <glm/glm.hpp>

// Only components
// TODO: Make check if components can be serialized
namespace CHEngine {

	struct UUID {
		UUID() : value(boost::uuids::nil_uuid()) {}
		UUID(const boost::uuids::uuid& u) : value(u) {}

		operator boost::uuids::uuid() const { return value; }
		operator boost::uuids::uuid() { return value; }
		UUID& operator=(const UUID& u) {
			value = u.value;
			return *this;
		}
        
        operator std::string() const { return boost::uuids::to_string(value); }

		bool operator==(const UUID& other) const { return value == other.value; }
		bool operator!=(const UUID& other) const { return value != other.value; }
		bool operator<(const UUID& other) const { return value < other.value; }

		bool IsValid() const { return value != boost::uuids::nil_uuid(); }
    private:
        boost::uuids::uuid value;
	};

	inline std::ostream& operator<<(std::ostream& os, const UUID& u) {
        os << static_cast<std::string>(u);
		return os;
	}

	inline std::size_t hash_value(const UUID& u) {
		return boost::hash<boost::uuids::uuid>()(static_cast<boost::uuids::uuid>(u));
	}

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

    struct ParentNodeComponent
    {
        UUID Value = boost::uuids::nil_uuid();
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
        CameraVariant Camera = PerspectiveCamera{};
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

    // TODO: Copying between scenes could be better
    using CopyableSceneComponents = ComponentGroup<
        TransformComponent,
        ParentNodeComponent,
        ColorComponent,
        VisibilityComponent,
        LightComponent,
        CameraComponent,
        LifetimeComponent,
        ScriptComponent,
        RigidBody3DComponent,
        MeshComponent,
        UIOverlayCanvasComponent,
        UIWorldCanvasComponent,
        UIRectTransformComponent,
        UIImageComponent,
        UITextComponent,
        UIPanelComponent,
        UIButtonComponent,
        UISliderComponent>;
}

template <>
struct fmt::formatter<CHEngine::UUID> : fmt::formatter<std::string> {
    auto format(const CHEngine::UUID& u, fmt::format_context& ctx) const {
        return fmt::formatter<std::string>::format(static_cast<std::string>(u), ctx);
    }
};
