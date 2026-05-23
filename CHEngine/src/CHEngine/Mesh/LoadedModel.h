#pragma once

#include "CHEngine/Mesh/MeshRef.h"

#include <string>

namespace CHEngine
{
	struct LoadedModel {
		MeshRef     mesh;
		std::string name;
		bool        success = false;
		std::string error;
	};
}
