#pragma once

#include <glm/glm.hpp>

#include "CTexture.h"

// 材质：常用参数在前，可选的法线贴图放最后，这样 {&tex, tint, uScale} 依然可用
struct CMaterial
{
	// 主色贴图；为空表示纯色
	const CTexture* texture = nullptr;

	// 颜色倾向，最终算 albedo 时乘上去
	glm::vec3 tint{ 1.0f };

	// UV 平铺密度：2 表示纹理重复 2 次
	float uScale = 1.0f;

	// 法向贴图（可选）
	const CTexture* normalMap = nullptr;

	// 法向贴图强度，1.0 为标准
	float normalStrength = 1.0f;

	// 指针非空还不够：加载失败时纹理 ID 是 0，这时要退回纯色
	bool HasTexture() const { return texture && texture->IsValid(); }
	bool HasNormalMap() const { return normalMap && normalMap->IsValid(); }
};
