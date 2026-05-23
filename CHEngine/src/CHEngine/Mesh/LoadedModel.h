#pragma once

#include "CHEngine/Mesh/MeshRef.h"

#include <string>
#include <vector>

namespace CHEngine
{
	struct LoadedModel {
		std::vector<MeshRef> meshes;
		std::string          name;
		bool                 success = false;
		std::string          error;
	};
}
