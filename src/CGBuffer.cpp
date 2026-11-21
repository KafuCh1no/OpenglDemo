#include "CGBuffer.h"

#include <glad/glad.h>

#include <algorithm>
#include <iostream>

CGBuffer::CGBuffer(int width, int height)
{
	m_width  = std::max(width, 1);
	m_height = std::max(height, 1);
	CreateFBO();
}

CGBuffer::~CGBuffer()
{
	DestroyFBO();
}

namespace
{
	// 建一张 RGBA16F 颜色纹理（GBuffer 里位置/法线要浮点精度）
	unsigned int CreateColorTexture(int width, int height)
	{
		unsigned int tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
			GL_RGBA, GL_FLOAT, nullptr);
		// 延迟着色按像素一一对应采样，用 NEAREST 避免相邻像素混色
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
		return tex;
	}
}

void CGBuffer::CreateFBO()
{
	m_albedoTex   = CreateColorTexture(m_width, m_height);
	m_normalTex   = CreateColorTexture(m_width, m_height);
	m_positionTex = CreateColorTexture(m_width, m_height);

	// 深度纹理（不是 renderbuffer）：SSAO 和体积雾都要采样
	glGenTextures(1, &m_depthTex);
	glBindTexture(GL_TEXTURE_2D, m_depthTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0,
		GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &m_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_albedoTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_normalTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_positionTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTex, 0);

	const GLenum attachments[3] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
	};
	glDrawBuffers(3, attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CGBuffer] FBO incomplete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CGBuffer::DestroyFBO()
{
	if (m_albedoTex)   glDeleteTextures(1, &m_albedoTex);
	if (m_normalTex)   glDeleteTextures(1, &m_normalTex);
	if (m_positionTex) glDeleteTextures(1, &m_positionTex);
	if (m_depthTex)    glDeleteTextures(1, &m_depthTex);
	if (m_fbo)         glDeleteFramebuffers(1, &m_fbo);

	m_albedoTex = m_normalTex = m_positionTex = m_depthTex = m_fbo = 0;
}

void CGBuffer::Resize(int width, int height)
{
	width  = std::max(width, 1);
	height = std::max(height, 1);
	if (width == m_width && height == m_height) return;

	m_width  = width;
	m_height = height;

	DestroyFBO();
	CreateFBO();
}

void CGBuffer::Begin()
{
	glGetIntegerv(GL_VIEWPORT, m_savedViewport);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);

	// 三张颜色图统一清成 (0,0,0,0)：albedo.a = 0 表示该像素没有几何
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
}

void CGBuffer::End()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(m_savedViewport[0], m_savedViewport[1],
		m_savedViewport[2], m_savedViewport[3]);
}
