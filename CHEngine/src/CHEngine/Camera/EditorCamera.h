#pragma once

#include "Camera.h"
#include "PerspectiveCamera.h"
#include "OrthographicCamera.h"
#include "Timestep.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace CHEngine {

	// Its not a camera itself but a class with camera + position + view_matrix
	class CHENGINE_API EditorCamera
	{
	public:
		EditorCamera();
		EditorCamera(float fov_degrees, float aspectRatio, float nearClip, float farClip);

		CameraVariant&       GetCamera()       { return m_Camera; }
		const CameraVariant& GetCamera() const { return m_Camera; }
		void SetCameraType(ECameraType cameraType);

		void SetViewportSize(float width, float height);

		const glm::mat4& GetViewMatrix()      const { return m_ViewMatrix; }
		const glm::mat4& GetProjectionMatrix() const { return CameraHelpers::GetProjection(m_Camera); }
		glm::mat4        GetViewProjection()   const { return GetProjectionMatrix() * m_ViewMatrix; }
		const ECameraType		 GetType()	const { return m_CameraType; }

		const glm::vec3& GetPosition() const { return m_Position; }

		float     GetYaw()   const { return CameraHelpers::GetYaw(m_Camera); }
		float     GetPitch() const { return CameraHelpers::GetPitch(m_Camera); }
		glm::vec3 GetForwardDirection() const { return CameraHelpers::GetForwardDirection(m_Camera); }
		glm::vec3 GetRightDirection()   const { return CameraHelpers::GetRightDirection(m_Camera); }
		glm::vec3 GetUpDirection()      const { return CameraHelpers::GetUpDirection(m_Camera); }
		glm::quat GetOrientation()      const { return CameraHelpers::GetOrientation(m_Camera); }

		void SetYaw(float yaw);
		void SetPitch(float pitch);
		void SetYawPitch(float yaw, float pitch);
		void SetPosition(const glm::vec3& position);

	private:
		void UpdateView();

	private:
		CameraVariant m_Camera = PerspectiveCamera{};
		glm::vec3     m_Position{ 0.0f };
		glm::mat4     m_ViewMatrix{ 1.0f };
		ECameraType	  m_CameraType = ECameraType::PERSPECTIVE;
	};

}
