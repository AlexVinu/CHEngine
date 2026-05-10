#pragma once

#include <glm/glm.hpp>

namespace CHEngine {

	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Color;
	};

}
