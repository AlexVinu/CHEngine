#pragma once

#include <Core.h>
#include "Material.h"
#include "Mesh.h"
#include <memory>
#include <string>
#include <vector>

namespace CHEngine {

	// Инстансы лежат на каждом Mesh (Mat); один и тот же Ref может шариться между мешами.
	struct LoadedModel {
		std::vector<Mesh>                        meshes;
		std::string                              name;
		bool                                     success = false;
		std::string                              error;
	};

	class CHENGINE_API ModelLoader
	{
	public:
		static LoadedModel Load(const std::string& filepath, ShaderHandle meshShader);

	private:
		static LoadedModel LoadOBJ(const std::string& filepath, ShaderHandle meshShader);
		static LoadedModel LoadGLTF(const std::string& filepath, ShaderHandle meshShader);
	};

}
