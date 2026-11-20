#include "CHDR.h"

#include <glad/glad.h>

#include <iostream>

CHDR::CHDR(int width, int height)
	: m_tonemapShader("assets/shaders/tonemap.vert", "assets/shaders/tonemap.frag")
{
	// 要求 draw 时绑定一个 VAO
	glGenVertexArrays(1, &m_quadVao);

	m_width  = (width  > 0) ? width  : 1;
	m_height = (height > 0) ? height : 1;

	CreateFBO();
}

CHDR::~CHDR()
{
	DestroyFBO();
	if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
}

void CHDR::CreateFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glGenTextures(1, &m_colorTex);
	// 自定义 FBO 不会自带深度缓冲，不挂会导致深度测试失效
	glGenRenderbuffers(1, &m_depthRbo);

	// 颜色附件：16 位浮点。用 RGBA 而不是 RGB —— RGB16F 在部分驱动上不可作为渲染目标
	glBindTexture(GL_TEXTURE_2D, m_colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// 全屏三角的 UV 会超出 [0,1] 一点，必须 clamp，否则边缘采样到别的像素
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	// 深度附件：只在 pass 内用，不需要采样，renderbuffer 比纹理省
	glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CHDR] FBO incomplete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CHDR::DestroyFBO()
{
	if (m_depthRbo) glDeleteRenderbuffers(1, &m_depthRbo);
	if (m_colorTex) glDeleteTextures(1, &m_colorTex);
	if (m_fbo)      glDeleteFramebuffers(1, &m_fbo);

	m_depthRbo = m_colorTex = m_fbo = 0;
}

// 窗口尺寸变化时重建纹理
void CHDR::Resize(int width, int height)
{
	if (width <= 0 || height <= 0) return;
	if (width == m_width && height == m_height) return;

	m_width  = width;
	m_height = height;

	// 重新分配存储，FBO 的 attachment 绑定关系不变
	glBindTexture(GL_TEXTURE_2D, m_colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void CHDR::Begin()
{
	glGetIntegerv(GL_VIEWPORT, m_savedViewport);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);
}

void CHDR::End()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(m_savedViewport[0], m_savedViewport[1],
		m_savedViewport[2], m_savedViewport[3]);
}

// 色调映射
void CHDR::Present()
{
	// 全屏三角：不参与深度测试、不剔除、不混合
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	m_tonemapShader.use();
	m_tonemapShader.setInt("hdrBuffer", 0);
	m_tonemapShader.setFloat("exposure", exposure);
	m_tonemapShader.setBool("uToneMapping", toneMapping);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_colorTex);

	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	// 恢复全局状态，ImGui 与后续帧都依赖这两个开关
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}
