#include "chepch.h"
#include "EditorCamera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <magic_enum/magic_enum.hpp>

namespace CHEngine {

	EditorCamera::EditorCamera()
	{
		UpdateView();
	}

	EditorCamera::EditorCamera(float fov_degrees, float aspectRatio, float nearClip, float farClip)
	{
		PerspectiveCamera persp;
		persp.SetPerspective(glm::radians(fov_degrees), nearClip, farClip);
		// аспект задаётся через viewport — height фиксируем, width = aspect*height
		persp.SetViewportSize(static_cast<uint32_t>(std::max(aspectRatio, 0.0001f) * 1000.0f), 1000u);
		m_Camera = std::move(persp);
		UpdateView();
	}

	void EditorCamera::SetCameraType(ECameraType cameraType)
	{
		switch (cameraType)
		{
		case ECameraType::PERSPECTIVE:
		{
			m_CameraType = cameraType;
			auto w = CameraHelpers::GetViewportWidth(m_Camera);
			auto h = CameraHelpers::GetViewportHeight(m_Camera);
			
			m_Camera = PerspectiveCamera{};
			CameraHelpers::SetViewportSize(m_Camera, w, h);
			break;
		}
		case ECameraType::ORTHOGRAPHIC : 
		{
			m_CameraType = cameraType;
			auto w = CameraHelpers::GetViewportWidth(m_Camera);
			auto h = CameraHelpers::GetViewportHeight(m_Camera);

			m_Camera = OrthographicCamera{};
			CameraHelpers::SetViewportSize(m_Camera, w, h);
			break;
		}
		default:
			CHE_CORE_ERROR("EDITOR_CAMERA: GIVEN WRONG TYPE {}", magic_enum::enum_name(cameraType));
			break;
		}
	}

	void EditorCamera::SetViewportSize(float width, float height)
	{
		CameraHelpers::SetViewportSize(m_Camera,
			static_cast<uint32_t>(std::max(width, 1.0f)),
			static_cast<uint32_t>(std::max(height, 1.0f)));
	}

	void EditorCamera::UpdateView()
	{
		const glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	void EditorCamera::SetYaw(float yaw)
	{
		CameraHelpers::SetYaw(m_Camera, yaw);
		UpdateView();
	}

	void EditorCamera::SetPitch(float pitch)
	{
		CameraHelpers::SetPitch(m_Camera, pitch);
		UpdateView();
	}

	void EditorCamera::SetYawPitch(float yaw, float pitch)
	{
		CameraHelpers::SetYawPitch(m_Camera, yaw, pitch);
		UpdateView();
	}

	void EditorCamera::SetPosition(const glm::vec3& position)
	{
		m_Position = position;
		UpdateView();
	}

}
