#include "chepch.h"
#include "ModelLoader.h"
#include "Log/Log.h"

#include <algorithm>
#include <filesystem>

namespace CHEngine {

	LoadedModel ModelLoader::Load(const std::string& filepath, ShaderHandle meshShader)
	{
		CHE_CORE_ASSERT(meshShader.IsValid(), "ModelLoader::Load — meshShader must be valid");

		std::string ext = std::filesystem::path(filepath).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".obj")
			return LoadOBJ(filepath, meshShader);
		else if (ext == ".glb" || ext == ".gltf")
			return LoadGLTF(filepath, meshShader);

		LoadedModel result;
		result.error = "Unsupported file format: " + ext;
		CHE_CORE_ERROR("ModelLoader: {0}", result.error.c_str());
		return result;
	}

} // namespace CHEngine
