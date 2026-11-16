#include "CModel.h"

#include <cstdint>
#include <iostream>
#include <unordered_map>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// 把 (v, vt, vn) 三个索引打包进一个 64 位整数，直接当哈希表的 key
// 每段占 21 位（可表示 0 ~ 2097151），三段互不重叠，所以打包结果唯一、不会碰撞
uint64_t MakeVertKey(int v, int vt, int vn)
{
	return ((uint64_t)(uint32_t)(v + 1) << 42)
		| ((uint64_t)(uint32_t)(vt + 1) << 21)
		| (uint64_t)(uint32_t)(vn + 1);
}

bool CModel::Load(const std::string& path,
	std::vector<float>& verts,
	std::vector<unsigned int>& indices)
{
	// 先清空
	verts.clear();
	indices.clear();

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;

	// mtl 里的贴图路径相对 obj 所在目录
	const std::string dir = path.substr(0, path.find_last_of("/\\") + 1);

	// 把“.obj文本”变成“三个原始数组 + 三元组索引表”
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn,
		path.c_str(), dir.c_str()))
	{
		std::cout << "[CModel] failed: " << path << "  " << warn << "\n";
		return false;
	}

	if (attrib.vertices.empty())
	{
		std::cout << "[CModel] no vertices: " << path << "\n";
		return false;
	}

	// 建立哈希表
	std::unordered_map<uint64_t, unsigned int> unique;
	unique.reserve(attrib.vertices.size() / 3);

	for (const auto& shape : shapes)
	{
		for (const auto& idx : shape.mesh.indices)
		{
			const uint64_t key = MakeVertKey(idx.vertex_index, idx.texcoord_index, idx.normal_index);

			auto it = unique.find(key);
			// 不是最后一个说明之前已经填进去了，直接复用
			if (it != unique.end())
			{
				indices.push_back(it->second);
				continue;
			}

			// 位置
			const float px = attrib.vertices[3 * idx.vertex_index + 0];
			const float py = attrib.vertices[3 * idx.vertex_index + 1];
			const float pz = attrib.vertices[3 * idx.vertex_index + 2];

			// 法线
			// 缺失兜底
			float nx = 0.0f, ny = 1.0f, nz = 0.0f;
			if (idx.normal_index >= 0)
			{
				nx = attrib.normals[3 * idx.normal_index + 0];
				ny = attrib.normals[3 * idx.normal_index + 1];
				nz = attrib.normals[3 * idx.normal_index + 2];
			}

			// UV 
			// 缺失兜底
			float u = 0.0f, v = 0.0f;
			if (idx.texcoord_index >= 0)
			{
				u = attrib.texcoords[2 * idx.texcoord_index + 0];
				v = attrib.texcoords[2 * idx.texcoord_index + 1];
			}

			// verts的新顶点编号，/11是因为每个顶点占11个float
			const unsigned int newIndex = (unsigned int)(verts.size() / 11);

			verts.push_back(px); verts.push_back(py); verts.push_back(pz);
			verts.push_back(nx); verts.push_back(ny); verts.push_back(nz);
			verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f);  // 顶点色填白
			verts.push_back(u);  verts.push_back(v);

			unique[key] = newIndex;
			indices.push_back(newIndex);
		}
	}

	std::cout << "[CModel] " << path
		<< "  verts=" << verts.size() / 11
		<< "  tris=" << indices.size() / 3 << "\n";

	return true;
}
