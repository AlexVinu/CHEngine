#include "chepch.h"
#include "Camera.h"
#include "PerspectiveCamera.h"
#include "OrthographicCamera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace CHEngine
{
	const glm::vec3 Camera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	const glm::vec3 Camera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	const glm::vec3 Camera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	const glm::quat Camera::GetOrientation() const
	{
		return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

	void Camera::SetViewportSize(uint32_t width, uint32_t height)
	{
		CHE_CORE_ASSERT(width > 0 && height > 0, "NO WIDTH AND HEIGHT");
		m_AspectRatio = (float)width / (float)height;
	}

	void Camera::SetYaw(float yaw)
	{
		m_Yaw = yaw;
	}

	void Camera::SetPitch(float pitch)
	{
		pitch = std::abs(pitch) > 89.f ? (pitch < 0 ? -89. : 89.) : pitch;
		m_Pitch = pitch;
	}

	void Camera::SetYawPitch(float yaw, float pitch)
	{
		SetYaw(yaw);
		SetPitch(pitch);
	}


	void Camera::SetNearClip(float near_clip)
	{
		m_Near = std::min(std::max(0.1f, near_clip), m_Far-10);
	}


	void Camera::SetFarClip(float far_clip)
	{
		m_Far = std::max(std::min(10000.f, far_clip), m_Near+10);
	}

	namespace CameraHelpers {

		const glm::mat4& GetProjection(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) -> const glm::mat4& { return c.GetProjection(); }, v);
		}

		void SetViewportSize(CameraVariant& v, uint32_t width, uint32_t height)
		{
			std::visit([&](auto& c) { c.SetViewportSize(width, height); }, v);
		}


		uint32_t GetViewportWidth(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetViewportWidth(); }, v);
		}
		uint32_t GetViewportHeight(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetViewportHeight(); }, v);
		}

		glm::vec3 GetForwardDirection(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetForwardDirection(); }, v);
		}

		glm::vec3 GetRightDirection(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetRightDirection(); }, v);
		}

		glm::vec3 GetUpDirection(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetUpDirection(); }, v);
		}

		glm::quat GetOrientation(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetOrientation(); }, v);
		}

		float GetPitch(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetPitch(); }, v);
		}

		float GetYaw(const CameraVariant& v)
		{
			return std::visit([](const Camera& c) { return c.GetYaw(); }, v);
		}

		void SetPitch(CameraVariant& v, float pitch)
		{
			std::visit([&](Camera& c) { c.SetPitch(pitch); }, v);
		}

		void SetYaw(CameraVariant& v, float yaw)
		{
			std::visit([&](Camera& c) { c.SetYaw(yaw); }, v);
		}

		void SetYawPitch(CameraVariant& v, float yaw, float pitch)
		{
			std::visit([&](Camera& c) { c.SetYawPitch(yaw, pitch); }, v);
		}

	} // namespace CameraHelpers

}
