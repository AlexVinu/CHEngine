#include "chepch.h"
#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace CHEngine {

	Camera::Camera(float fovDegrees, float nearClip, float farClip)
		: m_FOV(fovDegrees), m_Near(nearClip), m_Far(farClip)
	{
	}

	void Camera::SetPitch(float pitch)
	{
		// Clamp just inside ±90° — we allow up to ±89.9° so the top/bottom
		// view presets can get very close to vertical.  GetViewMatrix handles
		// the near-vertical degenerate case with a stable fallback up vector.
		if (pitch >  89.9f) pitch =  89.9f;
		if (pitch < -89.9f) pitch = -89.9f;
		m_Pitch = pitch;
	}

	glm::vec3 Camera::GetForward() const
	{
		glm::vec3 forward;
		forward.x = glm::cos(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch));
		forward.y = glm::sin(glm::radians(m_Pitch));
		forward.z = glm::sin(glm::radians(m_Yaw)) * glm::cos(glm::radians(m_Pitch));
		return glm::normalize(forward);
	}

	glm::vec3 Camera::GetRight() const
	{
		return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
	}

	glm::vec3 Camera::GetUp() const
	{
		return glm::normalize(glm::cross(GetRight(), GetForward()));
	}

	glm::mat4 Camera::GetViewMatrix() const
	{
		glm::vec3 fwd = GetForward();

		// When the camera looks nearly straight up or down, the standard world-up
		// (0,1,0) becomes (nearly) parallel to fwd → glm::lookAt produces NaN.
		// Derive a stable up vector from the yaw direction instead.
		glm::vec3 up;
		if (glm::abs(fwd.y) > 0.99f)
		{
			// Use the XZ facing direction as the screen "up" — this keeps the
			// view stable regardless of yaw when pitch is near ±90°.
			up = glm::vec3(glm::cos(glm::radians(m_Yaw)),
			               0.0f,
			               glm::sin(glm::radians(m_Yaw)));
		}
		else
		{
			up = glm::vec3(0.0f, 1.0f, 0.0f);
		}

		return glm::lookAt(m_Position, m_Position + fwd, up);
	}

	glm::mat4 Camera::GetProjectionMatrix(float aspectRatio) const
	{
		return glm::perspective(glm::radians(m_FOV), aspectRatio, m_Near, m_Far);
	}

	glm::mat4 Camera::GetViewProjectionMatrix(float aspectRatio) const
	{
		return GetProjectionMatrix(aspectRatio) * GetViewMatrix();
	}

}
