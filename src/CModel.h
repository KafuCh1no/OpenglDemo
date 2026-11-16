#pragma once

#include <string>
#include <vector>

// 加载 .obj 模型，输出统一顶点布局：pos3 | normal3 | color3 | uv2（共 11 float）
class CModel
{
public:
	static bool Load(const std::string& path,
		std::vector<float>& verts,
		std::vector<unsigned int>& indices);
};
