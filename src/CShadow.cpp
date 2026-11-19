#include "CShadow.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>

#include "CMesh.h"

CShadow::CShadow(int size): m_size(size), m_depthShader("assets/shaders/shadowdepth.vert", "assets/shaders/shadowdepth.frag")
{
	CreateDepthFBO();
}

CShadow::~CShadow()
{
	if (m_depthTex) glDeleteTextures(1, &m_depthTex);
	if (m_fbo)      glDeleteFramebuffers(1, &m_fbo);
}

void CShadow::CreateDepthFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glGenTextures(1, &m_depthTex);

	// 深度纹理：每个像素存「从灯看过去最近的深度」
	glBindTexture(GL_TEXTURE_2D, m_depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_size, m_size, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// 超出光源视野的采样点自动当作「最远」= 不在阴影里
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

	glBindTexture(GL_TEXTURE_2D, 0);

	// 挂到 FBO 上，并声明「不写颜色，只写深度」
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CShadow] FBO incomplete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 画深度图
void CShadow::Render(const std::vector<DrawItem>& items,
	const glm::vec3& lightPos,
	const glm::vec3& lightDir,
	float outerConeDeg,
	float nearZ,
	float farZ,
	float orthoHalfSize)
{
	glm::mat4 proj;
	// orthoHalfSize为0代表为透视投影
	if (orthoHalfSize == 0.0f)
	{
		// 透视投影
		float fovY = glm::radians(outerConeDeg * 2.0f);
		proj = glm::perspective(fovY, 1.0f, nearZ, farZ);
	}
	else
	{
		// 正交投影
		proj = glm::ortho(-orthoHalfSize, orthoHalfSize, -orthoHalfSize, orthoHalfSize, nearZ, farZ);
	}
	
	// 手电筒垂直朝下照时 lightDir 与 up 平行，cross 会退化成零向量，
	glm::vec3 up(0.0f, 1.0f, 0.0f);
	if (std::abs(glm::dot(glm::normalize(lightDir), up)) > 0.99f)
		up = glm::vec3(0.0f, 0.0f, 1.0f);

	glm::mat4 view = glm::lookAt(lightPos, lightPos + lightDir, up);
	m_lightSpace = proj * view;

	// 记下当前 viewport：深度图是方形的，画完必须恢复，否则主画面会缩到左下角
	GLint vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_size, m_size);
	glClear(GL_DEPTH_BUFFER_BIT);

	m_depthShader.use();
	m_depthShader.setMat4("lightSpaceMatrix", m_lightSpace);

	for (const DrawItem& it : items)
	{
		if (!it.mesh) continue;
		m_depthShader.setMat4("model", it.model);
		it.mesh->Draw();
	}
	// 恢复
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(vp[0], vp[1], vp[2], vp[3]);
}
