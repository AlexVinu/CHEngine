#include "chepch.h"
#include "SceneObject.h"

namespace CHEngine {

	uint32_t SceneObject::s_NextID = 1;

	SceneObject::SceneObject(const std::string& name)
		: Name(name), ID(s_NextID++)
	{
	}

	glm::mat4 Transform::GetMatrix() const
	{
		glm::mat4 m = glm::mat4(1.0f);
		m = glm::translate(m, Position);
		m = glm::rotate(m, glm::radians(Rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		m = glm::rotate(m, glm::radians(Rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		m = glm::rotate(m, glm::radians(Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		m = glm::scale(m, Scale);
		return m;
	}

	glm::mat4 Transform::GetNormalMatrix() const
	{
		return glm::transpose(glm::inverse(GetMatrix()));
	}

}
