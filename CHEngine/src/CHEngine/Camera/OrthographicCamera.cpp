#include "chepch.h"
#include "OrthographicCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace CHEngine {

	OrthographicCamera::OrthographicCamera()
	{
		RecalculateProjection();
	}

	void OrthographicCamera::SetOrthographic(float size, float nearClip, float farClip)
	{
		m_Size = size;
		m_Near = nearClip;
		m_Far = farClip;
		RecalculateProjection();
	}

	void OrthographicCamera::SetViewportSize(uint32_t width, uint32_t height)
	{
		Camera::SetViewportSize(width, height);
		RecalculateProjection();
	}

	void OrthographicCamera::RecalculateProjection()
	{
		float orthoLeft = -m_Size * m_AspectRatio * 0.5f;
		float orthoRight = m_Size * m_AspectRatio * 0.5f;
		float orthoBottom = -m_Size * 0.5f;
		float orthoTop = m_Size * 0.5f;

		m_Projection = glm::ortho(orthoLeft, orthoRight,
			orthoBottom, orthoTop, m_Near, m_Far);
	}
}
