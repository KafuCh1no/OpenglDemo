#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "CShader.h"

class CMesh;

// 阴影贴图（Shadow Mapping）
// 第一遍：把场景从「光源视角」画进一张离屏深度纹理
// 第二遍（主 Pass）：用同一个 lightSpaceMatrix 把片元变换到光源空间，
// 比较深度判断当前片元是否被遮挡
class CShadow
{
public:
	explicit CShadow(int size = 1024);
	~CShadow();

	// 持有 GL 资源（FBO / 纹理），禁止拷贝
	CShadow(const CShadow&) = delete;
	CShadow& operator=(const CShadow&) = delete;

	// 深度 Pass：把一组 mesh 从光源视角渲染进深度图
	void Render(const std::vector<const CMesh*>& meshes,
		const glm::vec3& lightPos,
		const glm::vec3& lightDir,
		float outerConeDeg,
		float nearZ,
		float farZ);

	unsigned int GetDepthTexture() const { return m_depthTex; }
	const glm::mat4& GetLightSpace() const { return m_lightSpace; }
	int GetSize() const { return m_size; }

private:
	void CreateDepthFBO();

	int m_size = 1024;

	unsigned int m_fbo = 0;
	unsigned int m_depthTex = 0;
	glm::mat4 m_lightSpace = glm::mat4(1.0f);

	CShader m_depthShader;
};
