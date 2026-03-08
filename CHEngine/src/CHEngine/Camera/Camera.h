#pragma once

#include <Core.h>

#include <glm/glm.hpp>

namespace CHEngine {

	class CHENGINE_API Camera
	{
	public:
		Camera(float fovDegrees = 45.0f, float nearClip = 0.1f, float farClip = 1000.0f);

		// --- Position & orientation ---
		void SetPosition(const glm::vec3& pos)  { m_Position = pos; }
		void SetYaw     (float yaw)              { m_Yaw   = yaw; }
		void SetPitch   (float pitch);           // clamped to [-89, 89]
		void SetFOV     (float fov)              { m_FOV = fov; }

		const glm::vec3& GetPosition() const { return m_Position; }
		float GetYaw()   const { return m_Yaw; }
		float GetPitch() const { return m_Pitch; }
		float GetFOV()   const { return m_FOV; }

		// --- Direction helpers ---
		glm::vec3 GetForward() const;
		glm::vec3 GetRight()   const;
		glm::vec3 GetUp()      const;

		// --- Matrices ---
		glm::mat4 GetViewMatrix()                        const;
		glm::mat4 GetProjectionMatrix(float aspectRatio) const;
		glm::mat4 GetViewProjectionMatrix(float aspectRatio) const;

	private:
		glm::vec3 m_Position = { 0.0f, 0.0f,  3.0f };

		float m_Yaw   = -90.0f;   // -90° → looking along -Z at startup
		float m_Pitch =   0.0f;

		float m_FOV  =  45.0f;
		float m_Near =   0.1f;
		float m_Far  = 100.0f;
	};

}
