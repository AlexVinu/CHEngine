#pragma once

#include "CHEngine/Mesh/Mesh.h"

#include <string>
#include <vector>

namespace CHEngine
{
	struct LoadedModel {
		std::vector<Mesh>                        meshes;
		std::string                              name;
		bool                                     success = false;
		std::string                              error;
	};
}