#include "CBloom.h"

#include <glad/glad.h>

#include <algorithm>
#include <iostream>

namespace
{
	unsigned int MakeColorFBO(int w, int h, unsigned int& tex)
	{
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		unsigned int fbo = 0;
		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "[CBloom] FBO incomplete!\n";

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return fbo;
	}
}

CBloom::CBloom(int width, int height)
	: m_extractShader("assets/shaders/bloomextract.vert", "assets/shaders/bloomextract.frag")
	, m_blurShader("assets/shaders/bloomblur.vert", "assets/shaders/bloomblur.frag")
{
	glGenVertexArrays(1, &m_quadVao);   // 全屏三角不传顶点，但 core profile 必须有 VAO
	Resize(width, height);
}

CBloom::~CBloom()
{
	DestroyBuffers();
	if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
}

void CBloom::DestroyBuffers()
{
	if (m_extractFbo) { glDeleteFramebuffers(1, &m_extractFbo); m_extractFbo = 0; }
	if (m_extractTex) { glDeleteTextures(1, &m_extractTex); m_extractTex = 0; }

	for (int i = 0; i < 2; ++i)
	{
		if (m_blurFbo[i]) { glDeleteFramebuffers(1, &m_blurFbo[i]); m_blurFbo[i] = 0; }
		if (m_blurTex[i]) { glDeleteTextures(1, &m_blurTex[i]); m_blurTex[i] = 0; }
	}

	m_resultTex = 0;
}

void CBloom::Resize(int width, int height)
{
	width  = std::max(width, 1);
	height = std::max(height, 1);

	const int ew = std::max(width / 2, 1);
	const int eh = std::max(height / 2, 1);
	const int bw = std::max(width / 4, 1);
	const int bh = std::max(height / 4, 1);

	if (ew == m_extractW && eh == m_extractH && bw == m_blurW && bh == m_blurH)
		return;

	DestroyBuffers();

	m_extractW = ew; m_extractH = eh;
	m_blurW    = bw; m_blurH    = bh;

	m_extractFbo = MakeColorFBO(m_extractW, m_extractH, m_extractTex);
	m_blurFbo[0] = MakeColorFBO(m_blurW, m_blurH, m_blurTex[0]);
	m_blurFbo[1] = MakeColorFBO(m_blurW, m_blurH, m_blurTex[1]);
}

void CBloom::DrawQuad() const
{
	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}

void CBloom::Render(unsigned int hdrColorTex)
{
	if (!enabled || hdrColorTex == 0 || m_extractFbo == 0) return;

	GLint savedViewport[4];
	glGetIntegerv(GL_VIEWPORT, savedViewport);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	// ---- ① 亮部提取，降到 1/2 分辨率 ----
	glBindFramebuffer(GL_FRAMEBUFFER, m_extractFbo);
	glViewport(0, 0, m_extractW, m_extractH);

	m_extractShader.use();
	m_extractShader.setInt("uHdr", 0);
	m_extractShader.setFloat("uThreshold", threshold);
	// 只从自发光（HDR 的 alpha）提取，避免手电筒扫过白墙时也出光晕
	m_extractShader.setBool("uOnlyEmissive", onlyEmissive);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrColorTex);
	DrawQuad();

	// ---- ② 反复 横向 + 纵向 高斯模糊，目标 1/4 分辨率 ----
	m_blurShader.use();
	m_blurShader.setInt("uSource", 1);

	unsigned int srcTex = m_extractTex;
	int srcW = m_extractW;
	int srcH = m_extractH;
	int dst = 0;

	for (int iter = 0; iter < std::max(iterations, 1); ++iter)
	{
		for (int pass = 0; pass < 2; ++pass)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, m_blurFbo[dst]);
			glViewport(0, 0, m_blurW, m_blurH);

			// pass 0 = 横向，pass 1 = 纵向
			const float dx = (pass == 0) ? radius / (float)srcW : 0.0f;
			const float dy = (pass == 0) ? 0.0f : radius / (float)srcH;
			m_blurShader.setFloat("uDirX", dx);
			m_blurShader.setFloat("uDirY", dy);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, srcTex);
			DrawQuad();

			srcTex = m_blurTex[dst];
			srcW = m_blurW;
			srcH = m_blurH;
			dst = 1 - dst;
		}
	}

	m_resultTex = srcTex;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}
