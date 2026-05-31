#include "chepch.h"
#include "../ModelLoader.h"
#include "Log/Log.h"

#include <filesystem>
#include <memory>
#include <vector>

#include "CHEngine/Application.h"
#include "FileSystem/FileSystem.h"

// GLTF/GLB loader implementation (header-only lib — compile exactly once here)
// STB_IMAGE_IMPLEMENTATION defined here so stbi_load is available for OBJ loader too
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace CHEngine {

	namespace {

	// Pak-aware tinygltf filesystem callbacks. All external buffer/image reads
	// (.gltf + .bin + textures) go through FileSystem (pak-first, disk fallback),
	// so exported games load models straight from game.chepak. In the editor no
	// pak is mounted, so every call falls through to disk transparently.
	tinygltf::FsCallbacks MakePakFsCallbacks()
	{
		tinygltf::FsCallbacks cb{};
		cb.user_data = nullptr;

		cb.FileExists = [](const std::string& path, void*) -> bool {
			return FileSystem::Exists(std::filesystem::path(path));
		};
		cb.ExpandFilePath = [](const std::string& path, void*) -> std::string {
			return path;
		};
		cb.ReadWholeFile = [](std::vector<unsigned char>* out, std::string* err,
		                      const std::string& path, void*) -> bool {
			Buffer buf = FileSystem::ReadFileBinary(std::filesystem::path(path));
			if (!buf || buf.Size == 0) {
				if (err) *err += "FileSystem: cannot read '" + path + "'\n";
				return false;
			}
			out->assign(buf.Data, buf.Data + buf.Size);
			return true;
		};
		cb.WriteWholeFile = [](std::string*, const std::string&,
		                       const std::vector<unsigned char>&, void*) -> bool {
			return false;  // загрузчик ничего не пишет
		};
		cb.GetFileSizeInBytes = [](size_t* filesize_out, std::string* err,
		                           const std::string& path, void*) -> bool {
			Buffer buf = FileSystem::ReadFileBinary(std::filesystem::path(path));
			if (!buf) {
				if (err) *err += "FileSystem: cannot stat '" + path + "'\n";
				return false;
			}
			*filesize_out = static_cast<size_t>(buf.Size);
			return true;
		};
		return cb;
	}

	Ref<MaterialInstance> CreateGltfMaterialInstance(const tinygltf::Model& model, int materialIndex,
	                                                             ShaderHandle meshShader)
	{
		auto base = MakeRef<Material>(meshShader);

		if (materialIndex < 0 || materialIndex >= static_cast<int>(model.materials.size()))
			return MaterialInstance::FromBase(base);

		const auto& mat = model.materials[materialIndex];
		int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;

		if (texIdx < 0)
		{
			auto extIt = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
			if (extIt != mat.extensions.end() && extIt->second.IsObject())
			{
				const auto& sg = extIt->second;
				if (sg.Has("diffuseTexture"))
				{
					const auto& dt = sg.Get("diffuseTexture");
					if (dt.IsObject() && dt.Has("index"))
						texIdx = dt.Get("index").GetNumberAsInt();
				}
			}
		}

		if (texIdx >= 0 && texIdx < static_cast<int>(model.textures.size()))
		{
			int imgIdx = model.textures[texIdx].source;
			if (imgIdx >= 0 && imgIdx < static_cast<int>(model.images.size()))
			{
				const auto& img = model.images[imgIdx];
				if (!img.image.empty() && img.width > 0 && img.height > 0)
				{
					base->DiffuseMap = CHEngine::Application::Get().Render().CreateTexture(
						img.image.data(),
						static_cast<uint32_t>(img.width),
						static_cast<uint32_t>(img.height),
						static_cast<uint32_t>(img.component));
				}
			}
		}
		return MaterialInstance::FromBase(base);
	}

	} // namespace

	ModelHandle ModelLoader::LoadGLTF(const std::filesystem::path& filepath, ShaderHandle meshShader)
	{
		LoadedModel result;
		result.name = filepath.stem().string();

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string warn, err;

		tinygltf::FsCallbacks fscb = MakePakFsCallbacks();
		loader.SetFsCallbacks(fscb);

		// Read the root file through FileSystem (pak-first), then parse from memory.
		// base_dir is the asset's directory so relative .bin/texture URIs resolve
		// against it; our FsCallbacks then route those reads through FileSystem too.
		Buffer rootData = FileSystem::ReadFileBinary(filepath);
		if (!rootData || rootData.Size == 0)
		{
			CHE_CORE_ERROR("Failed to read GLTF file: {}", filepath.string());
			return {};
		}
		const std::string baseDir = filepath.parent_path().generic_string();

		std::string ext = filepath.extension().string();
		bool ok = (ext == ".glb")
		    ? loader.LoadBinaryFromMemory(&model, &err, &warn,
		                                  rootData.Data, static_cast<unsigned int>(rootData.Size), baseDir)
		    : loader.LoadASCIIFromString(&model, &err, &warn,
		                                  rootData.As<char>(), static_cast<unsigned int>(rootData.Size), baseDir);

		if (!warn.empty())
			CHE_CORE_WARN("GLTF warning: {0}", warn.c_str());
		if (!err.empty())
			CHE_CORE_ERROR("GLTF error: {0}", err.c_str());

		if (!ok)
		{
			CHE_CORE_ERROR("Failed to load GLTF: {}", err);
			return {};
		}

		std::vector<Ref<MaterialInstance>> gltfMats;
		gltfMats.reserve(model.materials.size());
		for (size_t mi = 0; mi < model.materials.size(); ++mi)
			gltfMats.push_back(CreateGltfMaterialInstance(model, static_cast<int>(mi), meshShader));

		auto defaultMat = MaterialInstance::FromBase(MakeRef<Material>(meshShader));

		// Global pools for the whole model.
		std::vector<Vertex>   verticesAll;
		std::vector<uint32_t> indicesAll;
		std::vector<SubMesh>  subMeshes;
		std::vector<Ref<MaterialInstance>> matsOut;

		for (const auto& gltfMesh : model.meshes)
		{
			for (const auto& primitive : gltfMesh.primitives)
			{
				if (primitive.mode != TINYGLTF_MODE_TRIANGLES && primitive.mode != -1)
					continue;

				std::vector<Vertex> vertices;
				std::vector<uint32_t> indices;

				// ── Position ──────────────────────────────────────────────────
				auto posIt = primitive.attributes.find("POSITION");
				if (posIt == primitive.attributes.end())
					continue;

				const auto& posAccessor = model.accessors[posIt->second];
				const auto& posView     = model.bufferViews[posAccessor.bufferView];
				const auto& posBuf      = model.buffers[posView.buffer];
				const float* posData    = reinterpret_cast<const float*>(
					&posBuf.data[posView.byteOffset + posAccessor.byteOffset]);

				size_t vertexCount = posAccessor.count;
				vertices.resize(vertexCount);

				size_t posStride = posView.byteStride ? posView.byteStride / sizeof(float) : 3;
				for (size_t i = 0; i < vertexCount; i++)
				{
					vertices[i].Position = {
						posData[i * posStride],
						posData[i * posStride + 1],
						posData[i * posStride + 2]
					};
					vertices[i].Color = { 1.0f, 1.0f, 1.0f };
				}

				// ── Normal ────────────────────────────────────────────────────
				auto normIt = primitive.attributes.find("NORMAL");
				if (normIt != primitive.attributes.end())
				{
					const auto& normAccessor = model.accessors[normIt->second];
					const auto& normView     = model.bufferViews[normAccessor.bufferView];
					const auto& normBuf      = model.buffers[normView.buffer];
					const float* normData    = reinterpret_cast<const float*>(
						&normBuf.data[normView.byteOffset + normAccessor.byteOffset]);

					size_t normStride = normView.byteStride ? normView.byteStride / sizeof(float) : 3;
					for (size_t i = 0; i < vertexCount; i++)
					{
						vertices[i].Normal = {
							normData[i * normStride],
							normData[i * normStride + 1],
							normData[i * normStride + 2]
						};
					}
				}

				// ── TexCoord ──────────────────────────────────────────────────
				auto uvIt = primitive.attributes.find("TEXCOORD_0");
				if (uvIt != primitive.attributes.end())
				{
					const auto& uvAccessor = model.accessors[uvIt->second];
					const auto& uvView     = model.bufferViews[uvAccessor.bufferView];
					const auto& uvBuf      = model.buffers[uvView.buffer];
					const float* uvData    = reinterpret_cast<const float*>(
						&uvBuf.data[uvView.byteOffset + uvAccessor.byteOffset]);

					size_t uvStride = uvView.byteStride ? uvView.byteStride / sizeof(float) : 2;
					for (size_t i = 0; i < vertexCount; i++)
					{
						vertices[i].TexCoords = {
							uvData[i * uvStride],
							uvData[i * uvStride + 1]
						};
					}
				}

				// ── Index buffer ──────────────────────────────────────────────
				if (primitive.indices >= 0)
				{
					const auto& idxAccessor = model.accessors[primitive.indices];
					const auto& idxView     = model.bufferViews[idxAccessor.bufferView];
					const auto& idxBuf      = model.buffers[idxView.buffer];
					const uint8_t* idxData  = &idxBuf.data[idxView.byteOffset + idxAccessor.byteOffset];

					indices.resize(idxAccessor.count);

					if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
					{
						const uint16_t* data16 = reinterpret_cast<const uint16_t*>(idxData);
						for (size_t i = 0; i < idxAccessor.count; i++)
							indices[i] = static_cast<uint32_t>(data16[i]);
					}
					else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
					{
						const uint32_t* data32 = reinterpret_cast<const uint32_t*>(idxData);
						for (size_t i = 0; i < idxAccessor.count; i++)
							indices[i] = data32[i];
					}
					else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
					{
						for (size_t i = 0; i < idxAccessor.count; i++)
							indices[i] = static_cast<uint32_t>(idxData[i]);
					}
				}
				else
				{
					// No index buffer — sequential indices
					indices.resize(vertexCount);
					for (size_t i = 0; i < vertexCount; i++)
						indices[i] = static_cast<uint32_t>(i);
				}

				if (!vertices.empty() && !indices.empty())
				{
					const int matIdx = primitive.material;
					Ref<MaterialInstance> mat = (matIdx >= 0 && matIdx < static_cast<int>(gltfMats.size()))
					    ? gltfMats[static_cast<size_t>(matIdx)]
					    : defaultMat;

					// Append this primitive's vertices/indices to global pools,
					// shifting indices by current vertex count.
					const uint32_t vertexBase = static_cast<uint32_t>(verticesAll.size());
					const uint32_t indexStart = static_cast<uint32_t>(indicesAll.size());

					verticesAll.insert(verticesAll.end(), vertices.begin(), vertices.end());
					indicesAll.reserve(indicesAll.size() + indices.size());
					for (uint32_t idx : indices)
						indicesAll.push_back(idx + vertexBase);

					SubMesh sm{};
					sm.startIndex = indexStart;
					sm.indexCount = static_cast<uint32_t>(indices.size());
					sm.baseVertex = 0;
					subMeshes.push_back(sm);
					matsOut.push_back(std::move(mat));
				}
			}
		}

		if (verticesAll.empty() || subMeshes.empty())
		{
			CHE_CORE_WARN("No geometry found in GLTF file: {}", filepath.string());
			return {};
		}

		MeshLoader* meshLoader = Application::Get().Resources().GetMeshLoader();
		result.mesh = MeshRef{ meshLoader->GetOrCreate(verticesAll, indicesAll, subMeshes, std::move(matsOut)) };

		if (!result.mesh.IsValid())
		{
			CHE_CORE_WARN("GLTF load produced invalid mesh: {}", filepath.string());
			return {};
		}

		result.success = true;
		CHE_CORE_INFO("Loaded GLTF '{}': {} submesh(es), {} vertices",
		              result.name.c_str(), subMeshes.size(), verticesAll.size());
		return m_Models.Add(new LoadedModel(std::move(result)));
	}

} // namespace CHEngine
