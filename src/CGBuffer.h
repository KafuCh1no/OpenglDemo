#pragma once

// 延迟着色用的 G-buffer：
//   attachment 0 = albedo(rgb) + 是否几何(a)
//   attachment 1 = 世界空间法线
//   attachment 2 = 世界空间位置
//   depth        = 24 位深度纹理（SSAO / 体积雾要采样）
class CGBuffer
{
public:
	CGBuffer(int width, int height);
	~CGBuffer();

	CGBuffer(const CGBuffer&) = delete;
	CGBuffer& operator=(const CGBuffer&) = delete;

	void Resize(int width, int height);

	// 绑定 FBO、设置 viewport、清空三张颜色图和深度
	void Begin();
	// 回到默认帧缓冲，恢复进入前的 viewport
	void End();

	unsigned int GetFBO()             const { return m_fbo; }
	unsigned int GetAlbedoTexture()   const { return m_albedoTex; }
	unsigned int GetNormalTexture()   const { return m_normalTex; }
	unsigned int GetPositionTexture() const { return m_positionTex; }
	unsigned int GetDepthTexture()    const { return m_depthTex; }

	int Width()  const { return m_width; }
	int Height() const { return m_height; }

private:
	void CreateFBO();
	void DestroyFBO();

	unsigned int m_fbo         = 0;
	unsigned int m_albedoTex   = 0;
	unsigned int m_normalTex   = 0;
	unsigned int m_positionTex = 0;
	unsigned int m_depthTex    = 0;

	int m_width  = 0;
	int m_height = 0;

	int m_savedViewport[4] = { 0, 0, 0, 0 };
};
