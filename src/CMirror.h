#pragma once

#include <glm/glm.hpp>

#include "CShader.h"
#include "CGBuffer.h"

class CScene;
class CDeferred;

// 平面反射：
// 1) 把主相机关于镜面平面做镜像（反射矩阵），用一个斜近裁剪面（Lengyel）把镜面
//    背后的几何裁掉，把整个场景再渲染一遍到一张半分辨率纹理；
// 2) 主 Pass 之后，在镜面位置画一个小方片，用镜像相机的投影坐标采样那张纹理。
// 反射走和主画面同一条延迟管线（G-buffer + PBR + IBL），这样镜子里的亮度和
// 主画面一致，不会出现"镜子里一片黑"。
// 只支持一个平面（洗手间的镜子），不支持曲面/递归镜中镜。
class CMirror
{
public:
	CMirror(int width, int height);
	~CMirror();

	CMirror(const CMirror&) = delete;
	CMirror& operator=(const CMirror&) = delete;

	// 按窗口尺寸重建（内部按半分辨率分配）
	void Resize(int width, int height);

	// 设置镜面：世界坐标的中心点、法线（朝向房间）、半宽/半高
	void SetSurface(const glm::vec3& center, const glm::vec3& normal,
		float halfWidth, float halfHeight);

	// 反射 Pass：必须在主 Pass 的阴影图生成之后、主画面光照之前调用。
	// gbufferShader 写反射用的 G-buffer，deferred 负责把它照亮（复用主画面的掉
	// 延迟光照对象，IBL 参数保持一致）。
	void RenderReflection(CScene& scene, CShader& gbufferShader, CDeferred& deferred,
		const glm::mat4& projection, const glm::mat4& mainView,
		const glm::vec3& cameraPos);

	// 主 Pass 之后：把反射纹理贴到镜面上（需要主 Pass 的深度缓冲做遮挡）
	void Draw(const glm::mat4& projection, const glm::mat4& mainView);

	unsigned int GetTexture() const { return m_colorTex; }
	bool IsValid() const { return m_colorTex != 0 && m_depthRbo != 0; }

	// ---- 参数 ----
	bool  enabled = true;
	float tint      = 1.0f;     // 反射整体亮度（老镜子可以调暗一点）
	float edgeFade  = 0.06f;    // 边缘压暗宽度（镜框内侧的渐变）

private:
	void CreateFBO();
	void DestroyFBO();

	CGBuffer m_gbuf;   // 反射相机专用的半分辨率 G-buffer

	unsigned int m_fbo       = 0;
	unsigned int m_colorTex  = 0;
	unsigned int m_depthRbo  = 0;
	unsigned int m_quadVao   = 0;
	unsigned int m_quadVbo   = 0;

	int m_width  = 0;
	int m_height = 0;
	int m_screenW = 0;   // 窗口分辨率（gl_FragCoord 所在的坐标系）
	int m_screenH = 0;

	glm::vec3 m_center{ 0.0f, 0.0f, 0.0f };
	glm::vec3 m_normal{ 0.0f, 0.0f, 1.0f };
	float     m_halfW = 0.5f;
	float     m_halfH = 0.5f;

	glm::mat4 m_mirrorVP{ 1.0f };   // 反射相机用的 proj * view（画镜面片时要用）

	CShader m_shader;               // 镜面片
};
