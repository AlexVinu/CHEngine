#include "chepch.h"
#include "ModelLoader.h"
#include "Log/Log.h"

#include <algorithm>
#include <unordered_map>
#include <filesystem>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace CHEngine {

	LoadedModel ModelLoader::Load(const std::string& filepath,
	                              RenderResourceManager& resources)
	{
		std::string ext = std::filesystem::path(filepath).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".obj")
			return LoadOBJ(filepath, resources);
		else if (ext == ".glb" || ext == ".gltf")
			return LoadGLTF(filepath, resources);

		LoadedModel result;
		result.error = "Unsupported file format: " + ext;
		CHE_CORE_ERROR("ModelLoader: {0}", result.error.c_str());
		return result;
	}

	LoadedModel ModelLoader::LoadOBJ(const std::string& filepath,
	                                 RenderResourceManager& resources)
	{
		LoadedModel result;
		result.name = std::filesystem::path(filepath).stem().string();

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		std::string baseDir = std::filesystem::path(filepath).parent_path().string();
		if (!baseDir.empty()) baseDir += "/";

		bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
		                           filepath.c_str(), baseDir.c_str());

		if (!warn.empty())
			CHE_CORE_WARN("OBJ warning: {0}", warn.c_str());
		if (!err.empty())
			CHE_CORE_ERROR("OBJ error: {0}", err.c_str());

		if (!ok)
		{
			result.error = "Failed to load OBJ: " + err;
			return result;
		}

		for (const auto& shape : shapes)
		{
			std::vector<Vertex> vertices;
			std::vector<uint32_t> indices;

			struct VertexHash {
				size_t operator()(const std::tuple<int, int, int>& key) const {
					auto h1 = std::hash<int>{}(std::get<0>(key));
					auto h2 = std::hash<int>{}(std::get<1>(key));
					auto h3 = std::hash<int>{}(std::get<2>(key));
					return h1 ^ (h2 << 1) ^ (h3 << 2);
				}
			};
			std::unordered_map<std::tuple<int, int, int>, uint32_t, VertexHash> uniqueVertices;

			// Helper: get or create vertex, return its index
			auto getVertexIndex = [&](size_t faceOffset, int vertInFace,
			                          bool hasNormals, const glm::vec3& faceNormal) -> uint32_t
			{
				const auto& idx = shape.mesh.indices[faceOffset + vertInFace];
				auto key = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);

				auto it = uniqueVertices.find(key);
				if (it != uniqueVertices.end())
					return it->second;

				Vertex vert{};
				vert.Position = {
					attrib.vertices[3 * idx.vertex_index],
					attrib.vertices[3 * idx.vertex_index + 1],
					attrib.vertices[3 * idx.vertex_index + 2]
				};

				if (hasNormals && idx.normal_index >= 0)
				{
					vert.Normal = {
						attrib.normals[3 * idx.normal_index],
						attrib.normals[3 * idx.normal_index + 1],
						attrib.normals[3 * idx.normal_index + 2]
					};
				}
				else
				{
					vert.Normal = faceNormal;
				}

				if (idx.texcoord_index >= 0)
				{
					vert.TexCoords = {
						attrib.texcoords[2 * idx.texcoord_index],
						attrib.texcoords[2 * idx.texcoord_index + 1]
					};
				}

				vert.Color = { 1.0f, 1.0f, 1.0f };

				uint32_t newIdx = static_cast<uint32_t>(vertices.size());
				uniqueVertices[key] = newIdx;
				vertices.push_back(vert);
				return newIdx;
			};

			size_t indexOffset = 0;
			for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
			{
				int fv = shape.mesh.num_face_vertices[f];

				// Compute face normal if normals are missing
				glm::vec3 faceNormal(0.0f, 1.0f, 0.0f);
				bool hasNormals = false;

				if (fv >= 3)
				{
					auto& idx0 = shape.mesh.indices[indexOffset];
					hasNormals = (idx0.normal_index >= 0);

					if (!hasNormals)
					{
						auto& idx1 = shape.mesh.indices[indexOffset + 1];
						auto& idx2 = shape.mesh.indices[indexOffset + 2];

						glm::vec3 v0(attrib.vertices[3 * idx0.vertex_index],
						             attrib.vertices[3 * idx0.vertex_index + 1],
						             attrib.vertices[3 * idx0.vertex_index + 2]);
						glm::vec3 v1(attrib.vertices[3 * idx1.vertex_index],
						             attrib.vertices[3 * idx1.vertex_index + 1],
						             attrib.vertices[3 * idx1.vertex_index + 2]);
						glm::vec3 v2(attrib.vertices[3 * idx2.vertex_index],
						             attrib.vertices[3 * idx2.vertex_index + 1],
						             attrib.vertices[3 * idx2.vertex_index + 2]);

						glm::vec3 edge1 = v1 - v0;
						glm::vec3 edge2 = v2 - v0;
						float len = glm::length(glm::cross(edge1, edge2));
						if (len > 1e-6f)
							faceNormal = glm::cross(edge1, edge2) / len;
					}
				}

				// Fan triangulation: vertex 0 is the pivot
				uint32_t idx0 = getVertexIndex(indexOffset, 0, hasNormals, faceNormal);
				for (int v = 1; v < fv - 1; v++)
				{
					uint32_t idx1 = getVertexIndex(indexOffset, v, hasNormals, faceNormal);
					uint32_t idx2 = getVertexIndex(indexOffset, v + 1, hasNormals, faceNormal);
					indices.push_back(idx0);
					indices.push_back(idx1);
					indices.push_back(idx2);
				}

				indexOffset += fv;
			}

			if (!vertices.empty())
			{
				Mesh mesh;
				mesh.Build(resources, vertices, indices);

				// Load diffuse texture from material if available
				int matIdx = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[0];
				if (matIdx >= 0 && matIdx < (int)materials.size())
				{
					const std::string& texName = materials[matIdx].diffuse_texname;
					if (!texName.empty())
					{
						std::string texPath = baseDir + texName;
						int w, h, ch;
						stbi_set_flip_vertically_on_load(1);
						uint8_t* pixels = stbi_load(texPath.c_str(), &w, &h, &ch, 0);
						if (pixels)
						{
							mesh.DiffuseTexture = resources.CreateTexture(
								pixels, (uint32_t)w, (uint32_t)h, (uint32_t)ch);
							stbi_image_free(pixels);
						}
						else
						{
							CHE_CORE_WARN("OBJ: failed to load texture '{0}'", texPath.c_str());
						}
					}
				}

				result.meshes.push_back(std::move(mesh));
			}
		}

		result.success = !result.meshes.empty();
		if (result.success)
			CHE_CORE_INFO("Loaded OBJ '{0}': {1} mesh(es)", result.name.c_str(), result.meshes.size());
		else
			result.error = "No geometry found in OBJ file";

		return result;
	}

	LoadedModel ModelLoader::LoadGLTF(const std::string& filepath,
	                                  RenderResourceManager& resources)
	{
		LoadedModel result;
		result.name = std::filesystem::path(filepath).stem().string();

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string warn, err;

		std::string ext = std::filesystem::path(filepath).extension().string();
		bool ok = false;
		if (ext == ".glb")
			ok = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
		else
			ok = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);

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
				if (primitive.mode != TINYGLTF_MODE_TRIANGLES &&
				    primitive.mode != -1) // default is triangles
					continue;

				std::vector<Vertex> vertices;
				std::vector<uint32_t> indices;

				// Position
				auto posIt = primitive.attributes.find("POSITION");
				if (posIt == primitive.attributes.end())
					continue;

				const auto& posAccessor = model.accessors[posIt->second];
				const auto& posView = model.bufferViews[posAccessor.bufferView];
				const auto& posBuf = model.buffers[posView.buffer];
				const float* posData = reinterpret_cast<const float*>(
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

				// Normal
				auto normIt = primitive.attributes.find("NORMAL");
				if (normIt != primitive.attributes.end())
				{
					const auto& normAccessor = model.accessors[normIt->second];
					const auto& normView = model.bufferViews[normAccessor.bufferView];
					const auto& normBuf = model.buffers[normView.buffer];
					const float* normData = reinterpret_cast<const float*>(
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

				// TexCoord
				auto uvIt = primitive.attributes.find("TEXCOORD_0");
				if (uvIt != primitive.attributes.end())
				{
					const auto& uvAccessor = model.accessors[uvIt->second];
					const auto& uvView = model.bufferViews[uvAccessor.bufferView];
					const auto& uvBuf = model.buffers[uvView.buffer];
					const float* uvData = reinterpret_cast<const float*>(
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

				// Indices
				if (primitive.indices >= 0)
				{
					const auto& idxAccessor = model.accessors[primitive.indices];
					const auto& idxView = model.bufferViews[idxAccessor.bufferView];
					const auto& idxBuf = model.buffers[idxView.buffer];

					indices.resize(idxAccessor.count);
					const uint8_t* idxData = &idxBuf.data[idxView.byteOffset + idxAccessor.byteOffset];

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
					// No index buffer — generate sequential indices
					indices.resize(vertexCount);
					for (size_t i = 0; i < vertexCount; i++)
						indices[i] = static_cast<uint32_t>(i);
				}

				if (!vertices.empty() && !indices.empty())
				{
					Mesh mesh;
					mesh.Build(resources, vertices, indices);

					// Resolve diffuse texture from material.
					// Supports pbrMetallicRoughness and KHR_materials_pbrSpecularGlossiness.
					if (primitive.material >= 0 && primitive.material < (int)model.materials.size())
					{
						const auto& mat = model.materials[primitive.material];

						// Primary: standard PBR
						int texIdx = mat.pbrMetallicRoughness.baseColorTexture.index;

						// Fallback: KHR_materials_pbrSpecularGlossiness → diffuseTexture
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

}
