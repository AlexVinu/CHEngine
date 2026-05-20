#pragma once

#include "Camera.h"

namespace CHEngine {

	class CHENGINE_API OrthographicCamera : public Camera
	{
	public:
		OrthographicCamera();
		virtual ~OrthographicCamera() = default;

		void SetOrthographic(float size, float nearClip, float farClip);

		void SetViewportSize(uint32_t width, uint32_t height);

		float GetSize() const { return m_Size; }
		void SetSize(float size) { m_Size = size; RecalculateProjection(); }
		float GetNearClip() const { return m_Near; }
		void SetNearClip(float nearClip) { m_Near = nearClip; RecalculateProjection(); }
		float GetFarClip() const { return m_Far; }
		void SetFarClip(float farClip) { m_Far = farClip; RecalculateProjection(); }

	private:
		void RecalculateProjection();

		float m_Size = 10.0f;
	};
}
