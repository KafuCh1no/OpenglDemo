#include "CVolumetricFog.h"

#include <glad/glad.h>

#include <algorithm>
#include <iostream>

CVolumetricFog::CVolumetricFog(int width, int height)
	: m_shader("assets/shaders/volumetric.vert", "assets/shaders/volumetric.frag"),
	  m_blurShader("assets/shaders/volumetric.vert", "assets/shaders/fogblur.frag")
{
	glGenVertexArrays(1, &m_quadVao);

	// 半分辨率
	m_width  = std::max(width  / 2, 1);
	m_height = std::max(height / 2, 1);

	CreateFBO();
}

CVolumetricFog::~CVolumetricFog()
{
	DestroyFBO();
	if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
}

void CVolumetricFog::CreateFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glGenTextures(1, &m_fogTex);

	// rgb = 散射光（可能 > 1.0，所以用浮点），a = 透射率
	glBindTexture(GL_TEXTURE_2D, m_fogTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fogTex, 0);
	// 不挂深度：这一步不做深度测试，也不写深度（终点来自主 Pass 的深度纹理）

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CVolumetricFog] FBO incomplete!\n";

	// 模糊用的中间目标（和雾同尺寸）
	glGenFramebuffers(1, &m_blurFbo);
	glGenTextures(1, &m_blurTex);

	glBindTexture(GL_TEXTURE_2D, m_blurTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_blurTex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CVolumetricFog] blur FBO incomplete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CVolumetricFog::DestroyFBO()
{
	if (m_fogTex)  glDeleteTextures(1, &m_fogTex);
	if (m_fbo)     glDeleteFramebuffers(1, &m_fbo);
	if (m_blurTex) glDeleteTextures(1, &m_blurTex);
	if (m_blurFbo) glDeleteFramebuffers(1, &m_blurFbo);

	m_fogTex = m_fbo = m_blurTex = m_blurFbo = 0;
}

void CVolumetricFog::Resize(int width, int height)
{
	if (width <= 0 || height <= 0) return;

	const int w = std::max(width  / 2, 1);
	const int h = std::max(height / 2, 1);
	if (w == m_width && h == m_height) return;

	m_width  = w;
	m_height = h;

	glBindTexture(GL_TEXTURE_2D, m_fogTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, m_blurTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void CVolumetricFog::Render(unsigned int depthTex,
	const glm::mat4& invViewProj,
	const glm::vec3& cameraPos,
	unsigned int shadowMap,
	const glm::mat4& lightSpace,
	const glm::vec3& lightDir,
	const glm::vec3& lightColor,
	float lightIntensity)
{
	if (!m_fogTex || !depthTex || !shadowMap) return;

	GLint prevFbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
	glGetIntegerv(GL_VIEWPORT, m_savedViewport);

	// 切换绘制目标
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	// 初始化“无雾”
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// 纹理单元分配
	m_shader.use();
	m_shader.setInt("gDepth", 0);
	m_shader.setInt("shadowMap", 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowMap);

	// 变换矩阵
	m_shader.setMat4("invViewProj", invViewProj);
	m_shader.setMat4("lightSpace", lightSpace);
	m_shader.setVec3("cameraPos", cameraPos);

	// 光源
	m_shader.setVec3("lightDir", lightDir);
	m_shader.setVec3("lightColor", lightColor);
	m_shader.setFloat("lightIntensity", lightIntensity);

	// 体积雾参数
	m_shader.setFloat("densityBase", densityBase);
	m_shader.setFloat("heightFalloff", heightFalloff);
	m_shader.setFloat("fogBaseHeight", fogBaseHeight);
	m_shader.setFloat("maxDistance", maxDistance);
	m_shader.setFloat("phaseG", phaseG);


	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// 分离式高斯模糊：先水平写到 m_blurTex，再垂直写回 m_fogTex
	m_blurShader.use();
	m_blurShader.setInt("uSource", 0);
	m_blurShader.setVec2("uTexel", glm::vec2(1.0f / (float)m_width, 1.0f / (float)m_height));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_fogTex);
	glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo);
	glViewport(0, 0, m_width, m_height);
	m_blurShader.setVec2("uDir", glm::vec2(1.0f, 0.0f));
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindTexture(GL_TEXTURE_2D, m_blurTex);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	m_blurShader.setVec2("uDir", glm::vec2(0.0f, 1.0f));
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
	glViewport(m_savedViewport[0], m_savedViewport[1],
		m_savedViewport[2], m_savedViewport[3]);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}
