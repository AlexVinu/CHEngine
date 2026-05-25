#pragma once

#include <Core.h>

#include "Components.h"

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <boost/uuid/uuid.hpp>
#include <glm/glm.hpp>

namespace glm {
    inline void to_json(nlohmann::json& j, const vec3& v) {
        j = nlohmann::json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
    }
    inline void from_json(const nlohmann::json& j, vec3& v) {
        v.x = j.at("x").get<float>();
        v.y = j.at("y").get<float>();
        v.z = j.at("z").get<float>();
    }
}

namespace glm {
	inline void to_json(nlohmann::json& j, const vec2& v) {
		j = nlohmann::json{ {"x", v.x}, {"y", v.y} };
	}
	inline void from_json(const nlohmann::json& j, vec2& v) {
		v.x = j.at("x").get<float>();
		v.y = j.at("y").get<float>();
	}

	inline void to_json(nlohmann::json& j, const vec4& v) {
		j = nlohmann::json{ {"r", v.r}, {"g", v.g}, {"b", v.b}, {"a", v.a} };
	}
	inline void from_json(const nlohmann::json& j, vec4& v) {
		v.r = j.at("r").get<float>();
		v.g = j.at("g").get<float>();
		v.b = j.at("b").get<float>();
		v.a = j.at("a").get<float>();
	}
}

namespace CHEngine {

	inline void to_json(nlohmann::json& j, const UUID& uuid) {
		j = uuid.ToString(); 
	}
	inline void from_json(const nlohmann::json& j, UUID& uuid) {
		uuid = UUID::FromString(j.get<std::string>());
	}

	inline void to_json(nlohmann::json& j, const Transform& t) {
		j = nlohmann::json{
			{"position", t.Position},
			{"rotation", t.Rotation},
			{"scale",    t.Scale}
		};
	}
	inline void from_json(const nlohmann::json& j, Transform& t) {
		t.Position = j.at("position").get<glm::vec3>();
		t.Rotation = j.at("rotation").get<glm::vec3>();
		t.Scale    = j.at("scale").get<glm::vec3>();
	}

	inline void to_json(nlohmann::json& j, const Light& l) {
		j = nlohmann::json{ {"intensity", l.Intensity}, {"color", l.Color} };
	}
	inline void from_json(const nlohmann::json& j, Light& l) {
		l.Intensity = j.at("intensity").get<float>();
		l.Color     = j.at("color").get<glm::vec3>();
	}

	inline void to_json(nlohmann::json& j, const PhysicsRigidBodyDesc& d) {
		j = nlohmann::json{
			{"type",            static_cast<uint8_t>(d.Type)},
			{"mass",            d.Mass},
			{"linear_damping",  d.LinearDamping},
			{"angular_damping", d.AngularDamping},
			{"enable_gravity",  d.EnableGravity},
			{"static_friction", d.StaticFriction},
			{"dynamic_friction",d.DynamicFriction}
		};
	}
	inline void from_json(const nlohmann::json& j, PhysicsRigidBodyDesc& d) {
		d.Type            = static_cast<PhysicsBodyType>(j.value("type", uint8_t(PhysicsBodyType::Dynamic)));
		d.Mass            = j.value("mass", 1.0f);
		d.LinearDamping   = j.value("linear_damping", 0.0f);
		d.AngularDamping  = j.value("angular_damping", 0.05f);
		d.EnableGravity   = j.value("enable_gravity", true);
		d.StaticFriction  = j.value("static_friction", 0.5f);
		d.DynamicFriction = j.value("dynamic_friction", 0.5f);
	}

	inline void to_json(nlohmann::json& j, const PhysicsColliderShapeDesc& d) {
		j = nlohmann::json{
			{"type",         static_cast<uint8_t>(d.Type)},
			{"half_extents", d.HalfExtents},
			{"radius",       d.Radius},
			{"half_height",  d.HalfHeight}
		};
	}
	inline void from_json(const nlohmann::json& j, PhysicsColliderShapeDesc& d) {
		d.Type        = static_cast<PhysShapeType>(j.value("type", uint8_t(PhysShapeType::Box)));
		d.HalfExtents = j.value("half_extents", glm::vec3{0.5f});
		d.Radius      = j.value("radius", 0.5f);
		d.HalfHeight  = j.value("half_height", 0.5f);
	}

	inline void to_json(nlohmann::json& j, const PerspectiveCamera& c) {
		j = nlohmann::json{ {"fov", c.GetVerticalFOV()}, {"near", c.GetNearClip()}, {"far", c.GetFarClip()} };
	}
	inline void from_json(const nlohmann::json& j, PerspectiveCamera& c) {
		c.SetVerticalFOV(j.at("fov").get<float>());
		c.SetNearClip(j.at("near").get<float>());
		c.SetFarClip(j.at("far").get<float>());
	}

	inline void to_json(nlohmann::json& j, const OrthographicCamera& c) {
		j = nlohmann::json{ {"size", c.GetSize()}, {"near", c.GetNearClip()}, {"far", c.GetFarClip()} };
	}
	inline void from_json(const nlohmann::json& j, OrthographicCamera& c) {
		c.SetSize(j.at("size").get<float>());
		c.SetNearClip(j.at("near").get<float>());
		c.SetFarClip(j.at("far").get<float>());
	}

	inline void to_json(nlohmann::json& j, const CameraVariant& v) {
		std::visit([&](const auto& camera) {
			using T = std::decay_t<decltype(camera)>;
			if constexpr (std::is_same_v<T, PerspectiveCamera>) {
				j = nlohmann::json{ {"type", "perspective"}, {"data", camera} };
			}
			else {
				j = nlohmann::json{ {"type", "orthographic"}, {"data", camera} };
			}
			}, v);
	}
	inline void from_json(const nlohmann::json& j, CameraVariant& v) {
		std::string type = j.at("type").get<std::string>();
		if (type == "perspective") {
			v = j.at("data").get<PerspectiveCamera>();
		}
		else {
			v = j.at("data").get<OrthographicCamera>();
		}
	}
}

namespace CHEngine {


	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IDComponent, Value)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TagComponent, Name)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ParentNodeComponent, Value)


	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransformComponent, ObjectTransform, Dirty)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MeshComponent, SourcePath) 


	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ColorComponent, Color)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VisibilityComponent, Visible)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LightComponent, LightData)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CameraComponent, Camera, FixedAspectRatio, Primary, IsActive)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LifetimeComponent, RemainingSeconds, DestroyOnExpire)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ScriptComponent, Scripts)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RigidBody3DComponent, BodyDesc, ShapeDesc, SyncMode, SynchronisedTransform)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UIOverlayCanvasComponent, AnchorMin, AnchorMax, Position, Size, Pivot, SortOrder, Alpha)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UIWorldCanvasComponent, Size, Alpha, DoubleSided)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UIRectTransformComponent, AnchorMin, AnchorMax, Size, Pivot, Alpha, ZOrder)

	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UIImageComponent, Width, TexturePath, PreserveAspect, SlicedBorder)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UIPanelComponent, Width, BorderColor, BorderWidth, CornerRadius)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UIButtonComponent, Width, NormalColor, HoverColor, PressedColor, DisabledColor, Interactable, CornerRadius)
	NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UISliderComponent, Width, Value, Min, Max, BackgroundColor, FillColor, HandleColor, HandleSize, Interactable)

	inline void to_json(nlohmann::json& j, const UITextComponent& c) {
	j = nlohmann::json{
		{"Text", c.Text},
		{"FontPath", c.FontPath},
		{"FontSize", c.FontSize},
		{"Color", c.Color},
		{"HorizontalAlign", static_cast<uint8_t>(c.HorizontalAlign)},
		{"VerticalAlign", static_cast<uint8_t>(c.VerticalAlign)},
		{"Bold", c.Bold},
		{"Italic", c.Italic},
		{"WordWrap", c.WordWrap}
	};
	}
	inline void from_json(const nlohmann::json& j, UITextComponent& c) {
		c.Text = j.at("Text").get<StringID>();
		c.FontPath = j.at("FontPath").get<StringID>();
		c.FontSize = j.at("FontSize").get<float>();
		c.Color = j.at("Color").get<glm::vec4>();
		c.HorizontalAlign = static_cast<UITextComponent::HAlign>(j.at("HorizontalAlign").get<uint8_t>());
		c.VerticalAlign = static_cast<UITextComponent::VAlign>(j.at("VerticalAlign").get<uint8_t>());
		c.Bold = j.at("Bold").get<bool>();
		c.Italic = j.at("Italic").get<bool>();
		c.WordWrap = j.at("WordWrap").get<bool>();
	}

	template<typename T>
	void SerializeComponent(const entt::registry& reg, entt::entity e, nlohmann::json& entityJson) {
		if (reg.all_of<T>(e)) {
			auto typeName = std::string(entt::resolve<T>().info().name());
			entityJson["components"][typeName] = reg.get<T>(e); 
		}
	}

	template<typename T>
	void DeserializeComponent(entt::registry& reg, entt::entity e, const nlohmann::json& entityJson) {
		auto typeName = std::string(entt::resolve<T>().info().name());
		if (entityJson.contains("components") && entityJson["components"].contains(typeName)) {
			T component = entityJson["components"][typeName].get<T>(); 
			reg.emplace_or_replace<T>(e, std::move(component));
		}
	}

} // namespace CHEngine
