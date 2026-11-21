#include "CSSAO.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

namespace
{
	unsigned int MakeColorFBO(int w, int h, unsigned int& tex, GLenum internalFormat)
	{
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		unsigned int fbo = 0;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "[CSSAO] FBO incomplete!\n";

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return fbo;
	}
}

CSSAO::CSSAO(int width, int height)
	: m_ssaoShader("assets/shaders/ssao.vert", "assets/shaders/ssao.frag")
	, m_blurShader("assets/shaders/ssao.vert", "assets/shaders/ssaoblur.frag")
{
	// 半球采样核：大多集中在中心附近，边缘稀疏
	std::mt19937 rng(1337);
	std::uniform_real_distribution<float> rnd(0.0f, 1.0f);

	for (int i = 0; i < 32; ++i)
	{
		glm::vec3 s(rnd(rng) * 2.0f - 1.0f,
			rnd(rng) * 2.0f - 1.0f,
			rnd(rng));
		s = glm::normalize(s);
		s *= rnd(rng);

		float t = (float)i / 32.0f;
		float scale = 0.1f + 0.9f * t * t;
		m_kernel[i] = s * scale;
	}

	// 4x4 噪声纹理：每个像素的采样核随机旋转，把条带打散
	std::vector<glm::vec3> noise(16);
	for (int i = 0; i < 16; ++i)
		noise[i] = glm::vec3(rnd(rng) * 2.0f - 1.0f, rnd(rng) * 2.0f - 1.0f, 0.0f);

	glGenTextures(1, &m_noiseTex);
	glBindTexture(GL_TEXTURE_2D, m_noiseTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, noise.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenVertexArrays(1, &m_quadVao);
	Resize(width, height);
}

CSSAO::~CSSAO()
{
	DestroyBuffers();
	if (m_noiseTex) glDeleteTextures(1, &m_noiseTex);
	if (m_quadVao)  glDeleteVertexArrays(1, &m_quadVao);
}

void CSSAO::DestroyBuffers()
{
	if (m_aoFbo)   { glDeleteFramebuffers(1, &m_aoFbo);   m_aoFbo = 0; }
	if (m_aoTex)   { glDeleteTextures(1, &m_aoTex);       m_aoTex = 0; }
	if (m_blurFbo) { glDeleteFramebuffers(1, &m_blurFbo); m_blurFbo = 0; }
	if (m_blurTex) { glDeleteTextures(1, &m_blurTex);     m_blurTex = 0; }
	m_resultTex = 0;
}

void CSSAO::Resize(int width, int height)
{
	const int w = std::max(width / 2, 1);
	const int h = std::max(height / 2, 1);

	if (w == m_width && h == m_height) return;

	DestroyBuffers();
	m_width = w;
	m_height = h;

	m_aoFbo   = MakeColorFBO(w, h, m_aoTex,   GL_R16F);
	m_blurFbo = MakeColorFBO(w, h, m_blurTex, GL_R16F);
}

void CSSAO::Render(unsigned int depthTex, unsigned int normalTex,
	const glm::mat4& projection, const glm::mat4& view)
{
	if (!enabled || depthTex == 0 || normalTex == 0 || m_aoFbo == 0) return;

	GLint savedViewport[4];
	glGetIntegerv(GL_VIEWPORT, savedViewport);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	// 计算 SSAO
	glBindFramebuffer(GL_FRAMEBUFFER, m_aoFbo);
	glViewport(0, 0, m_width, m_height);

	m_ssaoShader.use();
	m_ssaoShader.setInt("gDepth", 0);
	m_ssaoShader.setInt("gNormal", 1);
	m_ssaoShader.setInt("uNoise", 2);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, normalTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_noiseTex);

	m_ssaoShader.setMat4("uProjection", projection);
	m_ssaoShader.setMat4("uView", view);
	m_ssaoShader.setMat4("uInvProjection", glm::inverse(projection));
	m_ssaoShader.setVec3Array("uKernel[0]", m_kernel, 32);
	m_ssaoShader.setFloat("uRadius", radius);
	m_ssaoShader.setFloat("uBias", bias);
	m_ssaoShader.setFloat("uStrength", strength);
	m_ssaoShader.setFloat("uPower", power);
	m_ssaoShader.setFloat("uNoiseScaleX", (float)m_width / 4.0f);
	m_ssaoShader.setFloat("uNoiseScaleY", (float)m_height / 4.0f);

	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// 模糊去噪
	glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo);
	glViewport(0, 0, m_width, m_height);

	m_blurShader.use();
	m_blurShader.setInt("gAO", 0);
	m_blurShader.setFloat("uTexelX", 1.0f / (float)m_width);
	m_blurShader.setFloat("uTexelY", 1.0f / (float)m_height);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_aoTex);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	m_resultTex = m_blurTex;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}
