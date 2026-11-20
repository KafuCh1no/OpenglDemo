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
	glGenTextures(1, &m_depthTex);

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

	// 深度附件：用纹理而不是 renderbuffer
	// 体积雾 / SSAO / 景深都要读它还原世界坐标，renderbuffer 取样不了
	// NEAREST 是必须的：插值出来的深度不对应任何真实表面
	glBindTexture(GL_TEXTURE_2D, m_depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_TEXTURE_2D, m_depthTex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CHDR] FBO incomplete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CHDR::DestroyFBO()
{
	if (m_depthTex) glDeleteTextures(1, &m_depthTex);
	if (m_colorTex) glDeleteTextures(1, &m_colorTex);
	if (m_fbo)      glDeleteFramebuffers(1, &m_fbo);

	m_depthTex = m_colorTex = m_fbo = 0;
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

	glBindTexture(GL_TEXTURE_2D, m_depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
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
void CHDR::Present(unsigned int fogTex)
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

	// 体积雾：rgba = (散射光, 透射率)
	m_tonemapShader.setBool("uHasFog", fogTex != 0);
	if (fogTex)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, fogTex);
		m_tonemapShader.setInt("fogBuffer", 1);
	}

	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	// 恢复全局状态，ImGui 与后续帧都依赖这两个开关
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}
