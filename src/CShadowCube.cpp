#include "CShadowCube.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "CMesh.h"

CShadowCube::CShadowCube(int size)
	: m_size(size)
	, m_depthShader("assets/shaders/shadowdepth_cube.vert",
		"assets/shaders/shadowdepth_cube.frag")
{
	CreateDepthCubemap();
}

CShadowCube::~CShadowCube()
{
	if (m_depthCube) glDeleteTextures(1, &m_depthCube);
	if (m_fbo)       glDeleteFramebuffers(1, &m_fbo);
}

void CShadowCube::CreateDepthCubemap()
{
	glGenFramebuffers(1, &m_fbo);
	glGenTextures(1, &m_depthCube);

	// 6 个面都要单独分配存储
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_depthCube);
	for (int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
			GL_DEPTH_COMPONENT24, m_size, m_size, 0,
			GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	// cubemap 的 S/T/R 三个方向都要设成 clamp，且不支持 border 颜色
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	// 先挂一个面，否则此刻 FBO 没有任何附件，状态检查会报 incomplete
	// （Render 里会按当前面逐个替换 depth attachment）
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_depthCube, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CShadowCube] FBO incomplete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CShadowCube::Render(const std::vector<CShadow::DrawItem>& items,
	const glm::vec3& lightPos,
	float nearZ,
	float farZ)
{
	m_far = farZ;

	// 每面 90° FOV，宽高比 1:1
	const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, nearZ, farZ);

	// 6 个面的朝向与对应的 up 向量
	// ±Y 两面必须换 up，否则 lookAt 的视线和 up 平行，cross 退化成零向量
	const glm::vec3 dirs[6] = {
		{ 1,  0,  0 }, { -1,  0,  0 },
		{ 0,  1,  0 }, { 0, -1,  0 },
		{ 0,  0,  1 }, { 0,  0, -1 },
	};
	const glm::vec3 ups[6] = {
		{ 0, -1,  0 }, { 0, -1,  0 },
		{ 0,  0,  1 }, { 0,  0, -1 },
		{ 0, -1,  0 }, { 0, -1,  0 },
	};

	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_size, m_size);

	m_depthShader.use();
	m_depthShader.setVec3("lightPos", lightPos);
	m_depthShader.setFloat("farPlane", farZ);

	// 点光阴影不能剔正面：壁炉这类模型是双面材质、几何有薄片
	// 剔掉正面后薄片处没有深度写入，光就漏过去了（影子出现缺口）
	glDisable(GL_CULL_FACE);

	for (int i = 0; i < 6; ++i)
	{
		const glm::mat4 view = glm::lookAt(lightPos, lightPos + dirs[i], ups[i]);

		// 把当前面挂到 FBO 的深度附件上
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_depthCube, 0);
		glClear(GL_DEPTH_BUFFER_BIT);

		m_depthShader.setMat4("shadowMatrix", proj * view);

		for (const CShadow::DrawItem& it : items)
		{
			if (!it.mesh) continue;
			m_depthShader.setMat4("model", it.model);
			it.mesh->Draw();
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(vp[0], vp[1], vp[2], vp[3]);
}
