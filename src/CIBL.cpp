#include "CIBL.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>

namespace
{
	constexpr int kEnvSize    = 512;   // 环境原图
	constexpr int kIrrSize    = 32;    // 漫反射辐照度（低频，小图足够）
	constexpr int kPrefSize   = 128;   // 镜面预过滤基级
	constexpr int kPrefMips   = 5;     // 128 -> 64 -> 32 -> 16 -> 8
	constexpr int kBrdfSize   = 256;   // BRDF LUT 分辨率

	const float kCubeVertices[] =
	{
		-1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f,
		 1.0f, 1.0f,-1.0f, -1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f, 1.0f,  1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
		 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f,-1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f, -1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f, -1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
		 1.0f, 1.0f, 1.0f,  1.0f, 1.0f,-1.0f,  1.0f,-1.0f,-1.0f,
		 1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f,-1.0f,  1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f,
		 1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,-1.0f,
		-1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,
		 1.0f,-1.0f, 1.0f, -1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f
	};

	// 建一张 cubemap：内部格式 RGBA16F，六个面先分配好
	unsigned int CreateCubemap(int size, bool withMipmaps)
	{
		unsigned int tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

		for (int i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA16F,
				size, size, 0, GL_RGBA, GL_FLOAT, nullptr);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
			withMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

		if (withMipmaps)
		{
			// 先把整条 mip 链分配出来，后面往各级 mip 里渲染
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}

		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		return tex;
	}

	void CheckFramebuffer(const char* tag)
	{
		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
			std::printf("[CIBL] %s framebuffer incomplete: 0x%x\n", tag, status);
	}
}

CIBL::CIBL()
	: m_envShader("assets/shaders/env.vert", "assets/shaders/env.frag")
	, m_irradianceShader("assets/shaders/env.vert", "assets/shaders/irradiance.frag")
	, m_prefilterShader("assets/shaders/env.vert", "assets/shaders/prefilter.frag")
	, m_brdfShader("assets/shaders/brdf.vert", "assets/shaders/brdf.frag")
{
	// ---- 立方体（36 个顶点，不用索引，够用了） ----
	glGenVertexArrays(1, &m_cubeVao);
	glGenBuffers(1, &m_cubeVbo);
	glBindVertexArray(m_cubeVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_cubeVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// ---- 全屏三角（BRDF LUT 用） ----
	glGenVertexArrays(1, &m_quadVao);

	// ---- 捕获环境用的投影和六个方向的观察矩阵 ----
	m_captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	const glm::vec3 origin(0.0f);
	m_captureViews[0] = glm::lookAt(origin, glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
	m_captureViews[1] = glm::lookAt(origin, glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f));
	m_captureViews[2] = glm::lookAt(origin, glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f));
	m_captureViews[3] = glm::lookAt(origin, glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f));
	m_captureViews[4] = glm::lookAt(origin, glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f));
	m_captureViews[5] = glm::lookAt(origin, glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f));

	// ---- 渲染到 cubemap 用的 FBO + 深度 renderbuffer ----
	glGenFramebuffers(1, &m_fbo);
	glGenRenderbuffers(1, &m_rbo);
}

CIBL::~CIBL()
{
	if (m_envMap)        glDeleteTextures(1, &m_envMap);
	if (m_irradianceMap) glDeleteTextures(1, &m_irradianceMap);
	if (m_prefilterMap)  glDeleteTextures(1, &m_prefilterMap);
	if (m_brdfLut)       glDeleteTextures(1, &m_brdfLut);

	if (m_cubeVbo) glDeleteBuffers(1, &m_cubeVbo);
	if (m_cubeVao) glDeleteVertexArrays(1, &m_cubeVao);
	if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
	if (m_fbo)     glDeleteFramebuffers(1, &m_fbo);
	if (m_rbo)     glDeleteRenderbuffers(1, &m_rbo);
}

void CIBL::DrawCube() const
{
	glBindVertexArray(m_cubeVao);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void CIBL::DrawFullscreenTriangle() const
{
	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}

void CIBL::Generate()
{
	// 记下调用前的状态，烘完要还回去
	GLint  savedViewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_VIEWPORT, savedViewport);
	const bool depthWasEnabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
	const bool cullWasEnabled  = glIsEnabled(GL_CULL_FACE)  == GL_TRUE;

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	// ================= 1. 把程序化环境画进 cubemap =================
	m_envMap = CreateCubemap(kEnvSize, true);

	glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kEnvSize, kEnvSize);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

	m_envShader.use();
	m_envShader.setMat4("uProjection", m_captureProjection);
	glViewport(0, 0, kEnvSize, kEnvSize);

	for (int face = 0; face < 6; ++face)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_envMap, 0);
		CheckFramebuffer("env");

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		m_envShader.setMat4("uView", m_captureViews[face]);
		DrawCube();
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, m_envMap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	// ================= 2. 漫反射辐照度（32x32） =================
	m_irradianceMap = CreateCubemap(kIrrSize, false);

	glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kIrrSize, kIrrSize);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

	m_irradianceShader.use();
	m_irradianceShader.setInt("uEnvMap", 0);
	m_irradianceShader.setMat4("uProjection", m_captureProjection);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_envMap);
	glViewport(0, 0, kIrrSize, kIrrSize);

	for (int face = 0; face < 6; ++face)
	{
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_irradianceMap, 0);
		CheckFramebuffer("irradiance");

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		m_irradianceShader.setMat4("uView", m_captureViews[face]);
		DrawCube();
	}

	// ================= 3. 镜面预过滤（128x128，5 级 mip） =================
	m_prefilterMap = CreateCubemap(kPrefSize, true);

	// 只用前 5 级 mip；后面更粗的 mip 没有内容，限制采样范围
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, kPrefMips - 1);

	m_prefilterShader.use();
	m_prefilterShader.setInt("uEnvMap", 0);
	m_prefilterShader.setMat4("uProjection", m_captureProjection);
	glBindTexture(GL_TEXTURE_CUBE_MAP, m_envMap);

	for (int mip = 0; mip < kPrefMips; ++mip)
	{
		const int size = kPrefSize >> mip;

		glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

		// 粗糙度按 mip 线性递增：0 = 镜面，1 = 完全模糊
		const float roughness = (float)mip / (float)(kPrefMips - 1);
		m_prefilterShader.setFloat("uRoughness", roughness);
		glViewport(0, 0, size, size);

		for (int face = 0; face < 6; ++face)
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_prefilterMap, mip);
			CheckFramebuffer("prefilter");

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			m_prefilterShader.setMat4("uView", m_captureViews[face]);
			DrawCube();
		}
	}

	// ================= 4. BRDF 积分查找表 =================
	glGenTextures(1, &m_brdfLut);
	glBindTexture(GL_TEXTURE_2D, m_brdfLut);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, kBrdfSize, kBrdfSize, 0,
		GL_RG, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// LUT 不需要深度，把 renderbuffer 摘掉
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLut, 0);
	CheckFramebuffer("brdf");

	glViewport(0, 0, kBrdfSize, kBrdfSize);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	m_brdfShader.use();
	DrawFullscreenTriangle();

	// ================= 收尾：还原状态 =================
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);

	if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (cullWasEnabled)  glEnable(GL_CULL_FACE);  else glDisable(GL_CULL_FACE);

	std::printf("[CIBL] env/irradiance/prefilter/BRDF generated (env=%d irr=%d pref=%d brdf=%d)\n",
		kEnvSize, kIrrSize, kPrefSize, kBrdfSize);
}
