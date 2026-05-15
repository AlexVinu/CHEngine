#pragma once

#include "Camera.h"
#include "Timestep.h"
#include "CHEngine/Events/Event.h"
#include "CHEngine/Events/MouseEvent.h"

#include <glm/glm.hpp>

namespace CHEngine {

	class CHENGINE_API EditorCamera : public Camera
	{
	public:
		EditorCamera();
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

		inline float GetFOV() const { return m_FOV; }
		inline void SetFOV(float fov)
		{
			m_FOV = fov;
			UpdateProjection();
		}
		inline float GetNearClip() const { return m_NearClip; }
		inline float GetFarClip() const { return m_FarClip; }
		inline void SetClippingPlanes(float near_clip, float far_clip)
		{
			m_NearClip = near_clip;
			m_FarClip = far_clip;
			UpdateProjection();
		}

		inline void SetViewportSize(float width, float height) { m_ViewportWidth = width; m_ViewportHeight = height; UpdateProjection(); }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetProjectionMatrix() const { return m_Projection; }
		glm::mat4 GetViewProjection() const { return m_Projection * m_ViewMatrix; }

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;
		const glm::vec3& GetPosition() const { return m_Position; }
		glm::quat GetOrientation() const;

		float GetPitch() const { return m_Pitch; }
		float GetYaw() const { return m_Yaw; }
		void SetYaw(float yaw);
		void SetPitch(float pitch);
		void SetYawPitch(float yaw, float pitch);
		void SetPosition(const glm::vec3& position);
	private:
		void UpdateProjection();
		void UpdateView();
	private:
		float m_FOV = 45.0f, m_AspectRatio = 1.778f, m_NearClip = 0.1f, m_FarClip = 1000.0f;

		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };

		float m_Pitch = 0.0f, m_Yaw = 0.0f;

		float m_ViewportWidth = 1280, m_ViewportHeight = 720;
	};

}
