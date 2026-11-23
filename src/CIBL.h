#pragma once

#include "CShader.h"

#include <glm/glm.hpp>

#include <string>

// 基于图像的光照（IBL）资源：
//   1. 程序化环境 cubemap（暗房间 + 月光 + 壁炉余光）—— 中间产物，不缓存
//   2. 漫反射辐照度 cubemap（32x32，低频）
//   3. 镜面预过滤 cubemap（128x128 + 5 级 mip，按 roughness 模糊）
//   4. BRDF 积分查找表（256x256，split-sum 的第二项）
//
// 2/3/4 会被烘焙到 assets/cache/*.bin，下次启动直接读文件，跳过整个烘焙过程。
class CIBL
{
public:
	CIBL();
	~CIBL();

	CIBL(const CIBL&) = delete;
	CIBL& operator=(const CIBL&) = delete;

	// 需要 GL 上下文已经创建（构造函数之后、主循环之前调用一次）
	// 有缓存就直接加载，没有则烘焙并把结果写进缓存
	void Generate();

	// 忽略缓存，强制重新烘焙并覆盖缓存文件
	void Rebake();

	// 缓存目录（默认 assets/cache）
	void SetCacheDir(const std::string& dir) { m_cacheDir = dir; }

	unsigned int GetEnvMap()        const { return m_envMap; }   // 缓存命中时为 0，运行时不用它
	unsigned int GetIrradianceMap() const { return m_irradianceMap; }
	unsigned int GetPrefilterMap()  const { return m_prefilterMap; }
	unsigned int GetBrdfLUT()       const { return m_brdfLut; }

	bool  enabled   = true;
	float intensity = 1.0f;   // 环境光倍率：0 关闭，1 默认，可以拉到 2~3 看效果

private:
	void Bake();

	bool LoadCached();
	bool SaveCache() const;

	std::string Path(const char* name) const { return m_cacheDir + "/" + name; }

	void DrawCube() const;
	void DrawFullscreenTriangle() const;

	unsigned int m_cubeVao = 0;
	unsigned int m_cubeVbo = 0;
	unsigned int m_quadVao = 0;

	unsigned int m_fbo = 0;
	unsigned int m_rbo = 0;

	unsigned int m_envMap        = 0;
	unsigned int m_irradianceMap = 0;
	unsigned int m_prefilterMap  = 0;
	unsigned int m_brdfLut       = 0;

	CShader m_envShader;
	CShader m_irradianceShader;
	CShader m_prefilterShader;
	CShader m_brdfShader;

	glm::mat4 m_captureProjection = glm::mat4(1.0f);
	glm::mat4 m_captureViews[6]   = { glm::mat4(1.0f) };

	std::string m_cacheDir = "assets/cache";
};
