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
		// Clamp to avoid gimbal-lock flip at ±90°
		if (pitch >  89.0f) pitch =  89.0f;
		if (pitch < -89.0f) pitch = -89.0f;
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
		return glm::lookAt(m_Position, m_Position + GetForward(), glm::vec3(0.0f, 1.0f, 0.0f));
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
