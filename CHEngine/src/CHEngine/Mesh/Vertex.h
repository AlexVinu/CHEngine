#pragma once

#include <glm/glm.hpp>

namespace CHEngine {
	// Structure without paddings
	struct Vertex {
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoords;
		glm::vec3 Color;
	};
	static_assert(sizeof(Vertex) == (sizeof(glm::vec3) * 3 + sizeof(glm::vec2)),
		"Vertex structure has unexpected padding!");
}
