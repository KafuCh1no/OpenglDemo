#pragma once

#include <glm/glm.hpp>

#include "CShader.h"

// Bloom：亮部提取 + 可分离高斯模糊
// 流程：HDR 纹理 -> 半分辨率提亮 -> 1/4 分辨率横模糊 -> 纵模糊 ->（来回两轮）
class CBloom
{
public:
	CBloom(int width, int height);
	~CBloom();

	CBloom(const CBloom&) = delete;
	CBloom& operator=(const CBloom&) = delete;

	void Resize(int width, int height);

	// hdrColorTex : CHDR 的颜色纹理（线性 HDR）
	void Render(unsigned int hdrColorTex);

	unsigned int GetTexture() const { return m_resultTex; }

	// ---- 参数 ----
	bool  enabled   = true;
	float threshold = 5.0f;    // 亮度阈值
	float strength  = 0.8f;    // 叠加到 HDR 上的强度
	float radius    = 1.5f;    // 模糊半径倍率
	int   iterations = 2;      // 来回模糊的轮数，越多越糊

private:
	void CreateBuffers();
	void DestroyBuffers();
	void DrawQuad() const;

	int m_extractW = 0, m_extractH = 0;   // 1/2 分辨率
	int m_blurW    = 0, m_blurH    = 0;   // 1/4 分辨率

	unsigned int m_extractFbo = 0, m_extractTex = 0;
	unsigned int m_blurFbo[2] = { 0, 0 };
	unsigned int m_blurTex[2] = { 0, 0 };
	unsigned int m_resultTex = 0;

	unsigned int m_quadVao = 0;

	CShader m_extractShader;
	CShader m_blurShader;
};
