#pragma once

#include <glm/glm.hpp>

#include "CShader.h"

// 体积雾（屏幕空间 raymarching）
// 从相机沿视线步进，每一步采样密度和光照，累积散射光与透射率。
// 输出 RGBA16F：rgb = 散射光，a = 透射率，由 CHDR::Present 合成到画面。
class CVolumetricFog
{
public:
	CVolumetricFog(int width, int height);
	~CVolumetricFog();

	CVolumetricFog(const CVolumetricFog&) = delete;
	CVolumetricFog& operator=(const CVolumetricFog&) = delete;

	// 按窗口尺寸重建（内部按半分辨率分配）
	void Resize(int width, int height);

	// 渲染雾，结果写进内部纹理
	// depthTex : 主 Pass 的深度纹理（必须已渲染完）
	// shadowMap / lightSpace : 平行光的阴影图与矩阵
	void Render(unsigned int depthTex,
		const glm::mat4& invViewProj,
		const glm::vec3& cameraPos,
		unsigned int shadowMap,
		const glm::mat4& lightSpace,
		const glm::vec3& lightDir,
		const glm::vec3& lightColor,
		float lightIntensity);

	unsigned int GetTexture() const { return m_fogTex; }
	bool IsValid() const { return m_fogTex != 0; }

	// ---- 参数（ImGui 可调）----
	bool  enabled       = true;
	float densityBase   = 0.20f;    // 整体浓度
	float heightFalloff = 0.35f;    // 高度衰减速度
	float fogBaseHeight = 0.0f;     // 从这个高度开始衰减
	float maxDistance   = 30.0f;    // 最远步进距离
	float phaseG        = 0.6f;     // 相函数：越大越聚成光束

private:
	void CreateFBO();
	void DestroyFBO();

	unsigned int m_fbo     = 0;
	unsigned int m_fogTex  = 0;
	unsigned int m_quadVao = 0;

	int m_width  = 0;
	int m_height = 0;

	int m_savedViewport[4] = { 0, 0, 0, 0 };

	CShader m_shader;
};
