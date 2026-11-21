#pragma once

#include "CShader.h"

// FXAA：后期抗锯齿
// 输入是色调映射之后的 LDR 图像，只采颜色，不碰深度/法线
class CFXAA
{
public:
	CFXAA(int width, int height);
	~CFXAA();

	CFXAA(const CFXAA&) = delete;
	CFXAA& operator=(const CFXAA&) = delete;

	// 只记录尺寸（用来算 texel size），没有需要重建的 FBO
	void Resize(int width, int height);

	// 把 srcTex 抗锯齿后画到默认帧缓冲，viewport 用当前窗口尺寸
	void Render(unsigned int srcTex);

	bool enabled = true;

private:
	unsigned int m_quadVao = 0;
	int m_width  = 1;
	int m_height = 1;

	CShader m_shader;
};
