#pragma once

#include <glm/glm.hpp>

#include "CShader.h"

// SSAO：屏幕空间环境光遮蔽
// 输入：深度纹理 + 世界空间法线纹理（MRT 输出）
// 输出：半分辨率的遮蔽图（1 = 无遮蔽，0 = 全遮蔽），再做一次模糊去噪
class CSSAO
{
public:
	CSSAO(int width, int height);
	~CSSAO();

	CSSAO(const CSSAO&) = delete;
	CSSAO& operator=(const CSSAO&) = delete;

	void Resize(int width, int height);

	void Render(unsigned int depthTex,
		unsigned int normalTex,
		const glm::mat4& projection,
		const glm::mat4& view);

	unsigned int GetTexture() const { return m_resultTex; }

	// ---- 可调参数 ----
	bool  enabled  = true;
	float radius   = 1.5f;    // 采样半径
	float bias     = 0.025f;  // 深度偏移，防自遮蔽
	float strength = 1.0f;    // 遮蔽强度
	float power    = 2.0f;    // 遮蔽对比度：越大暗角越深、越集中

private:
	void CreateBuffers();
	void DestroyBuffers();

	int m_width  = 0;
	int m_height = 0;

	unsigned int m_aoFbo   = 0, m_aoTex   = 0;
	unsigned int m_tempFbo = 0, m_tempTex = 0;   // 水平模糊的中间结果
	unsigned int m_blurFbo = 0, m_blurTex = 0;   // 垂直模糊的最终结果
	unsigned int m_resultTex = 0;

	unsigned int m_noiseTex = 0;
	unsigned int m_quadVao  = 0;

	glm::vec3 m_kernel[32];

	CShader m_ssaoShader;
	CShader m_blurShader;
};
