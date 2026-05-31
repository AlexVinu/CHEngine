#include "chepch.h"
#include "../ModelLoader.h"
#include "Log/Log.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <sstream>
#include <unordered_map>

#include "CHEngine/Mesh/Material.h"
#include "CHEngine/ResourceManager/ResourceManager.h"
#include "CHEngine/Application.h"
#include "FileSystem/FileSystem.h"

// OBJ loader implementation (header-only lib — compile exactly once here)
#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#include <tiny_obj_loader.h>

namespace CHEngine {

	namespace {

	// Reads .mtl files through FileSystem (pak-first, disk fallback) so exported
	// OBJ models resolve their materials from game.chepak. In the editor no pak is
	// mounted, so the read falls through to disk.
	class PakMaterialReader : public tinyobj::MaterialReader
	{
	public:
		explicit PakMaterialReader(std::string baseDir) : m_BaseDir(std::move(baseDir)) {}

		bool operator()(const std::string& matId,
		                std::vector<tinyobj::material_t>* materials,
		                std::map<std::string, int>* matMap,
		                std::string* warn, std::string* err) override
		{
			const std::string path = m_BaseDir + matId;
			std::string text = FileSystem::ReadFileText(std::filesystem::path(path));
			if (text.empty())
			{
				if (warn) *warn += "PakMaterialReader: cannot read mtl '" + path + "'\n";
				return true;  // отсутствующий .mtl — не фатально для tinyobj
			}
			std::istringstream ms(text);
			tinyobj::LoadMtl(matMap, materials, &ms, warn, err);
			return true;
		}

	private:
		std::string m_BaseDir;
	};

	} // namespace

	ModelHandle ModelLoader::LoadOBJ(const std::filesystem::path& filepath, ShaderHandle meshShader)
	{
		LoadedModel* result = new LoadedModel();
		result->name = filepath.stem().string();

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		std::string baseDir = filepath.parent_path().generic_string();
		if (!baseDir.empty()) baseDir += "/";

		// Read the .obj through FileSystem (pak-first), parse from memory; .mtl
		// files resolve through PakMaterialReader (also pak-aware).
		std::string objText = FileSystem::ReadFileText(filepath);
		if (objText.empty())
		{
			CHE_CORE_ERROR("Failed to read OBJ file: {}", filepath.string());
			delete result;
			return {};
		}
		std::istringstream objStream(objText);
		PakMaterialReader matReader(baseDir);

		bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
		                           &objStream, &matReader);

		if (!warn.empty())
			CHE_CORE_WARN("OBJ warning: {0}", warn.c_str());
		if (!err.empty())
			CHE_CORE_ERROR("OBJ error: {0}", err.c_str());

		if (!ok)
		{
			CHE_CORE_ERROR("Failed to load OBJ: {}", err);
			delete result;
			return {};
		}

		std::unordered_map<int, Ref<MaterialInstance>> objMatByIdx;

		auto getOrCreateObjMaterial = [&](int matIdx) -> Ref<MaterialInstance> {
			auto it = objMatByIdx.find(matIdx);
			if (it != objMatByIdx.end())
				return it->second;

			Ref<MaterialInstance> inst;
			if (matIdx < 0 || matIdx >= static_cast<int>(materials.size()))
			{
				inst = MaterialInstance::FromBase(std::make_shared<Material>(meshShader));
			}
			else
			{
				auto base = std::make_shared<Material>(meshShader);
				const std::string& texName = materials[static_cast<size_t>(matIdx)].diffuse_texname;
				if (!texName.empty())
				{
					std::string texPath = baseDir + texName;
					TextureHandle texHandle = Application::Get().Resources().Load<TextureHandle>(std::filesystem::path(texPath));
					if (texHandle.IsValid())
					{
						base->DiffuseMapPath = texPath;
						base->DiffuseMap = texHandle;
					}
					else
					{
						CHE_CORE_WARN("OBJ: failed to load texture '{0}'", texPath.c_str());
					}
				}
				inst = MaterialInstance::FromBase(base);
			}
			objMatByIdx[matIdx] = inst;
			return inst;
		};

		// One global vertex pool deduplicated across all shapes/materials.
		std::vector<Vertex> vertices;
		// One global index pool. Per-material draw ranges go into subMeshes.
		// We bucket indices by material into temporary buffers, then concatenate.
		std::unordered_map<int, std::vector<uint32_t>> indicesByMat;

		struct VertexHash {
			size_t operator()(const std::tuple<int, int, int>& key) const {
				auto h1 = std::hash<int>{}(std::get<0>(key));
				auto h2 = std::hash<int>{}(std::get<1>(key));
				auto h3 = std::hash<int>{}(std::get<2>(key));
				return h1 ^ (h2 << 1) ^ (h3 << 2);
			}
		};
		std::unordered_map<std::tuple<int, int, int>, uint32_t, VertexHash> uniqueVertices;

		auto getVertexIndex = [&](const tinyobj::shape_t& shape, size_t faceOffset, int vertInFace,
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

		for (const auto& shape : shapes)
		{
			size_t indexOffset = 0;
			for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
			{
				int fv = shape.mesh.num_face_vertices[f];

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

				const int matIdx = (f < shape.mesh.material_ids.size())
					? shape.mesh.material_ids[f]
					: -1;
				auto& bucket = indicesByMat[matIdx];

				uint32_t i0 = getVertexIndex(shape, indexOffset, 0, hasNormals, faceNormal);
				for (int v = 1; v < fv - 1; v++)
				{
					uint32_t i1 = getVertexIndex(shape, indexOffset, v,     hasNormals, faceNormal);
					uint32_t i2 = getVertexIndex(shape, indexOffset, v + 1, hasNormals, faceNormal);
					bucket.push_back(i0);
					bucket.push_back(i1);
					bucket.push_back(i2);
				}

				indexOffset += fv;
			}
		}

		if (vertices.empty() || indicesByMat.empty())
		{
			CHE_CORE_WARN("No geometry found in OBJ file: {}", filepath.string());
			delete result;
			return {};
		}

		// Concatenate per-material buckets into one IB; build submeshes + materials in parallel.
		std::vector<uint32_t> indices;
		std::vector<SubMesh>  subMeshes;
		std::vector<Ref<MaterialInstance>> matsOut;
		for (auto& kv : indicesByMat)
		{
			if (kv.second.empty()) continue;
			SubMesh sm{};
			sm.startIndex = static_cast<uint32_t>(indices.size());
			sm.indexCount = static_cast<uint32_t>(kv.second.size());
			sm.baseVertex = 0;
			indices.insert(indices.end(), kv.second.begin(), kv.second.end());
			subMeshes.push_back(sm);
			matsOut.push_back(getOrCreateObjMaterial(kv.first));
		}

		MeshLoader* loader = Application::Get().Resources().GetMeshLoader();
		result->mesh = MeshRef{ loader->GetOrCreate(vertices, indices, subMeshes, std::move(matsOut)) };

		if (!result->mesh.IsValid())
		{
			CHE_CORE_WARN("OBJ load produced invalid mesh: {}", filepath.string());
			delete result;
			return {};
		}

		CHE_CORE_INFO("Loaded OBJ '{}': {} submesh(es), {} vertices",
		              result->name.c_str(), subMeshes.size(), vertices.size());
		return m_Models.Add(result);
	}

} // namespace CHEngine
