#pragma once

#include <glm/glm.hpp>

#include "CTexture.h"

// 材质
struct CMaterial
{
	// 主色贴图；为空表示纯色
	const CTexture* texture = nullptr;
	// 颜色倾向，最终算 albedo 时乘上去
	glm::vec3 tint{ 1.0f };
	// UV 平铺密度：2 表示纹理重复 2 次
	float uScale = 1.0f;
	// 法向贴图
	const CTexture* normalMap = nullptr;
	// 法向贴图强度，1.0 为标准
	float normalStrength = 1.0f;
	// PBR：金属度（0 = 电介质，1 = 纯金属）与粗糙度（0 = 镜面，1 = 全漫反射）
	float metallic  = 0.0f;
	float roughness = 0.55f;

	// 金属度/粗糙度贴图：glTF 打包格式，G = roughness，B = metallic
	const CTexture* metallicRoughnessMap = nullptr;

	// B 通道是否可用
	bool mrUseMetallic = true;

	// 加载失败时纹理要退回纯色
	bool HasTexture() const { return texture && texture->IsValid(); }
	bool HasNormalMap() const { return normalMap && normalMap->IsValid(); }
	bool HasMRMap() const { return metallicRoughnessMap && metallicRoughnessMap->IsValid(); }
};
