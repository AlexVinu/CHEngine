#include "chepch.h"
#include "ModelLoader.h"
#include "Log/Log.h"

#include <filesystem>

// GLTF/GLB loader implementation (header-only lib — compile exactly once here)
// STB_IMAGE_IMPLEMENTATION defined here so stbi_load is available for OBJ loader too
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace CHEngine {

	LoadedModel ModelLoader::LoadGLTF(const std::string& filepath,
	                                  RenderResourceManager& resources)
	{
		LoadedModel result;
		result.name = std::filesystem::path(filepath).stem().string();

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string warn, err;

		std::string ext = std::filesystem::path(filepath).extension().string();
		bool ok = (ext == ".glb")
		    ? loader.LoadBinaryFromFile(&model, &err, &warn, filepath)
		    : loader.LoadASCIIFromFile(&model, &err, &warn, filepath);

		if (!warn.empty())
			CHE_CORE_WARN("GLTF warning: {0}", warn.c_str());
		if (!err.empty())
			CHE_CORE_ERROR("GLTF error: {0}", err.c_str());

		if (!ok)
		{
			result.error = "Failed to load GLTF: " + err;
			return result;
		}

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
					Mesh mesh;
					mesh.Build(resources, vertices, indices);

					// ── Diffuse texture ───────────────────────────────────────
					if (primitive.material >= 0 && primitive.material < (int)model.materials.size())
					{
						const auto& mat = model.materials[primitive.material];
						int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;

						// Fallback: KHR_materials_pbrSpecularGlossiness
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

						if (texIdx >= 0 && texIdx < (int)model.textures.size())
						{
							int imgIdx = model.textures[texIdx].source;
							if (imgIdx >= 0 && imgIdx < (int)model.images.size())
							{
								const auto& img = model.images[imgIdx];
								if (!img.image.empty() && img.width > 0 && img.height > 0)
								{
									mesh.DiffuseTexture = resources.CreateTexture(
										img.image.data(),
										(uint32_t)img.width,
										(uint32_t)img.height,
										(uint32_t)img.component);
								}
							}
						}
					}

					result.meshes.push_back(std::move(mesh));
				}
			}
		}

		result.success = !result.meshes.empty();
		if (result.success)
			CHE_CORE_INFO("Loaded GLTF '{0}': {1} mesh(es)", result.name.c_str(), result.meshes.size());
		else
			result.error = "No geometry found in GLTF file";

		return result;
	}

} // namespace CHEngine
