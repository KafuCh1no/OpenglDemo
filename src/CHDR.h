#pragma once

#include "CShader.h"

// HDR 离屏渲染 + 色调映射
class CHDR
{
public:
	CHDR(int width, int height);
	~CHDR();

	// 持有 GL 资源（FBO / 纹理），禁止拷贝
	CHDR(const CHDR&) = delete;
	CHDR& operator=(const CHDR&) = delete;

	// 窗口尺寸变化时重建纹理；尺寸没变则直接返回
	void Resize(int width, int height);

	// 进入 HDR Pass：绑定离屏 FBO 并设置 viewport
	void Begin();
	// 离开 HDR Pass：回到默认帧缓冲，恢复进入前的 viewport
	void End();

	// 色调映射：把 HDR 纹理铺满当前帧缓冲，应在 End() 之后调用
	// fogTex != 0 时把体积雾合成进来（rgb = 散射光，a = 透射率）
	void Present(unsigned int fogTex = 0, unsigned int bloomTex = 0, unsigned int aoTex = 0);

	int Width()  const { return m_width; }
	int Height() const { return m_height; }

	// 深度纹理：主 Pass 结束后可被子系统（体积雾 / SSAO / 景深）采样
	unsigned int GetColorTexture() const { return m_colorTex; }
	// HDR FBO 本身：延迟着色要把 G-buffer 的深度拷进来
	unsigned int GetFBO() const { return m_fbo; }
	unsigned int m_normalTex = 0;   // 法线缓冲（MRT，SSAO 采样）
	unsigned int GetDepthTexture() const { return m_depthTex; }
	unsigned int GetNormalTexture() const { return m_normalTex; }   // SSAO 用：世界空间法线
	// 色调映射的输出（LDR，8 位），给 FXAA 当输入
	unsigned int GetLDRTexture() const { return m_ldrTex; }

	// ---- 色调映射参数（ImGui 直接改）----
	float exposure    = 1.0f;
	float bloomStrength = 0.8f;   // Bloom 叠加强度
	float aoStrength   = 1.0f;    // SSAO 叠加强度
	// false = 直通：只做 sRGB 编码，画面与改造前一致
	// true  = Reinhard 压缩，高光不再死白
	bool  toneMapping = true;

	// HDR 总开关：false 时主循环完全绕过本类，直接画到窗口
	bool  enabled     = true;

private:
	unsigned int m_fbo      = 0;
	unsigned int m_colorTex = 0;
	unsigned int m_depthTex = 0;   // 纹理而不是 renderbuffer：体积雾要采样
	unsigned int m_ldrFbo   = 0;   // 色调映射的结果，供 FXAA 采样
	unsigned int m_ldrTex   = 0;
	unsigned int m_quadVao  = 0;   // 全屏三角不传顶点，但 core profile 必须有 VAO

	int m_width  = 0;
	int m_height = 0;

	int m_savedViewport[4] = { 0, 0, 0, 0 };

	CShader m_tonemapShader;

	void CreateFBO();
	void DestroyFBO();
};
