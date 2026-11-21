#include "CFXAA.h"

#include <glad/glad.h>

#include <algorithm>

CFXAA::CFXAA(int width, int height)
	: m_shader("assets/shaders/fxaa.vert", "assets/shaders/fxaa.frag")
{
	glGenVertexArrays(1, &m_quadVao);
	Resize(width, height);
}

CFXAA::~CFXAA()
{
	if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
}

void CFXAA::Resize(int width, int height)
{
	m_width  = std::max(width, 1);
	m_height = std::max(height, 1);
}

void CFXAA::Render(unsigned int srcTex)
{
	if (srcTex == 0) return;

	// 直接写窗口，viewport 保持调用者设置的值（窗口尺寸）
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	m_shader.use();
	m_shader.setInt("uSource", 0);
	m_shader.setFloat("uTexelX", 1.0f / (float)m_width);
	m_shader.setFloat("uTexelY", 1.0f / (float)m_height);
	m_shader.setBool("uEnabled", enabled);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, srcTex);

	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	// 恢复全局状态，ImGui 依赖这两个开关
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
}
