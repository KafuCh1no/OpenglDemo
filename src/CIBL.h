#pragma once

#include "CShader.h"

#include <glm/glm.hpp>

// 基于图像的光照（IBL）资源：
//   1. 程序化环境 cubemap（暗房间 + 月光 + 壁炉余光）
//   2. 漫反射辐照度 cubemap（32x32，低频）
//   3. 镜面预过滤 cubemap（128x128 + 5 级 mip，按 roughness 模糊）
//   4. BRDF 积分查找表（256x256，split-sum 的第二项）
//
// 全部在 Generate() 里一次性烘出来，运行时只是采样三张图。
class CIBL
{
public:
	CIBL();
	~CIBL();

	CIBL(const CIBL&) = delete;
	CIBL& operator=(const CIBL&) = delete;

	// 需要 GL 上下文已经创建（构造函数之后、主循环之前调用一次）
	void Generate();

	unsigned int GetEnvMap()        const { return m_envMap; }
	unsigned int GetIrradianceMap() const { return m_irradianceMap; }
	unsigned int GetPrefilterMap()  const { return m_prefilterMap; }
	unsigned int GetBrdfLUT()       const { return m_brdfLut; }

	bool  enabled   = true;
	float intensity = 1.0f;   // 环境光倍率：0 关闭，1 默认，可以拉到 2~3 看效果

private:
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
};
