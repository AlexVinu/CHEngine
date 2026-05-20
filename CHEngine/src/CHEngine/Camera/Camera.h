#pragma once

#include <Core.h>
#include <glm/glm.hpp>
#include <variant>

namespace CHEngine {

	enum class ECameraType : int8_t {
		PERSPECTIVE,
		ORTHOGRAPHIC
	};

	class CHENGINE_API Camera
	{
	public:
		Camera() = default;
		Camera(const glm::mat4& projection)
			: m_Projection(projection) {
		}

		virtual ~Camera() = default;

		const glm::mat4& GetProjection() const { return m_Projection; }
		const glm::vec3 GetUpDirection() const;
		const glm::vec3 GetRightDirection() const;
		const glm::vec3 GetForwardDirection() const;
		const glm::quat GetOrientation() const;

		const uint32_t GetViewportHeight() const { return m_ViewportHeight; }
		const uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		const float GetNearClip() const { return m_Near; }
		const float GetFarClip() const { return m_Far; }


		void SetViewportSize(uint32_t width, uint32_t height);

		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		void SetYaw(float yaw);
		void SetPitch(float pitch);
		void SetYawPitch(float yaw, float pitch);
		void SetNearClip(float near_clip);
		void SetFarClip(float far_clip);

	protected:
		glm::mat4 m_Projection = glm::mat4(1.0f);

		float m_Pitch = 0.0f, m_Yaw = 0.0f;
		uint32_t m_ViewportWidth = 1280, m_ViewportHeight = 720;
		float m_AspectRatio = 1280.0f / 720.0f;
		float m_Near = -1000.0f, m_Far = 1000.0f;
	};

	class PerspectiveCamera;
	class OrthographicCamera;

	using CameraVariant = std::variant<PerspectiveCamera, OrthographicCamera>;

	namespace CameraHelpers {

		CHENGINE_API const glm::mat4& GetProjection(const CameraVariant& v);
		CHENGINE_API void      SetViewportSize(CameraVariant& v, uint32_t width, uint32_t height);
		CHENGINE_API uint32_t  GetViewportWidth(const CameraVariant& v);
		CHENGINE_API uint32_t  GetViewportHeight(const CameraVariant& v);
		CHENGINE_API glm::vec3 GetForwardDirection(const CameraVariant& v);
		CHENGINE_API glm::vec3 GetRightDirection(const CameraVariant& v);
		CHENGINE_API glm::vec3 GetUpDirection(const CameraVariant& v);
		CHENGINE_API glm::quat GetOrientation(const CameraVariant& v);
		CHENGINE_API float     GetPitch(const CameraVariant& v);
		CHENGINE_API float     GetYaw(const CameraVariant& v);
		CHENGINE_API void      SetPitch(CameraVariant& v, float pitch);
		CHENGINE_API void      SetYaw(CameraVariant& v, float yaw);
		CHENGINE_API void      SetYawPitch(CameraVariant& v, float yaw, float pitch);

	} // namespace CameraHelpers

}
