#pragma once

#include "CShader.h"

// 延迟着色的光照 Pass：全屏三角，逐像素从 G-buffer 取数据算光照。
// 几何（gbuffer.frag）只光栅化一遍，这里再统一算一次光照。
class CDeferred
{
public:
	CDeferred(int width, int height);
	~CDeferred();

	CDeferred(const CDeferred&) = delete;
	CDeferred& operator=(const CDeferred&) = delete;

	// 画到当前绑定的帧缓冲（通常是 CHDR 的 HDR FBO）
	void Render(unsigned int albedoTex, unsigned int normalTex, unsigned int positionTex);

	// 主循环通过它上传灯光 / 阴影 uniform（CScene::SetupDeferredLighting）
	CShader& Shader() { return m_shader; }

	bool enabled = true;

	// Cook-Torrance 漫反射带 1/PI。1.0 = 纯物理值；
	// 之前用 3.14 补偿亮度，代价是直射光比 IBL 亮 π 倍
	float pbrGain = 1.0f;

	// 环境光照（IBL）：三张烘焙好的纹理由 CIBL 提供，0 表示还没生成
	unsigned int iblIrradiance = 0;
	unsigned int iblPrefilter  = 0;
	unsigned int iblBrdf       = 0;
	bool  iblEnabled   = true;
	float envIntensity = 1.0f;

private:
	unsigned int m_quadVao = 0;
	CShader      m_shader;
};
