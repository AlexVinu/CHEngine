#include "chepch.h"
#include "PerspectiveCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace CHEngine {

	PerspectiveCamera::PerspectiveCamera()
	{
		m_Near = 0.1f;
		m_Far  = 1000.0f;
		RecalculateProjection();
	}

	void PerspectiveCamera::SetPerspective(float verticalFOV, float nearClip, float farClip)
	{
		m_FOV = verticalFOV;
		m_Near = nearClip;
		m_Far = farClip;
		RecalculateProjection();
	}

	void PerspectiveCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		Camera::SetViewportSize(width, height);
		RecalculateProjection();
	}

	void PerspectiveCamera::RecalculateProjection()
	{
		m_Projection = glm::perspective(m_FOV, m_AspectRatio, m_Near, m_Far);
	}
}
