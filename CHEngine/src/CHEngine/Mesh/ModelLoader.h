#pragma once

#include <Core.h>
#include "Mesh.h"
#include <string>
#include <vector>

namespace CHEngine {

	struct LoadedModel {
		std::vector<Mesh> meshes;
		std::string name;
		bool success = false;
		std::string error;
	};

	class CHENGINE_API ModelLoader
	{
	public:
		static LoadedModel Load(const std::string& filepath,
		                        RenderResourceManager& resources);

	private:
		static LoadedModel LoadOBJ(const std::string& filepath,
		                           RenderResourceManager& resources);
		static LoadedModel LoadGLTF(const std::string& filepath,
		                            RenderResourceManager& resources);
	};

}
