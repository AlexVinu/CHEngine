#pragma once

#include <Core.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

#include "CHEngine/Mesh/Mesh.h"
#include "CHEngine/Scene/Light.h"

namespace CHEngine {

	struct CHENGINE_API Transform
	{
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in degrees
		glm::vec3 Scale    = { 1.0f, 1.0f, 1.0f };

		glm::mat4 GetMatrix() const;
		glm::mat4 GetNormalMatrix() const;
	};

	class CHENGINE_API SceneObject
	{
	public:
		SceneObject(const std::string& name = "Object");

		std::string Name;
		uint32_t    ID;

		Transform   ObjectTransform;

		std::vector<Mesh> Meshes;
		glm::vec4 Color = { 0.8f, 0.8f, 0.8f, 1.0f };
		bool Visible = true;

		// Источник света (Type == None → объект не является светом)
		Light LightData;

	private:
		static uint32_t s_NextID;
	};

}
