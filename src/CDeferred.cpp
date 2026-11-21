#include "CDeferred.h"

#include <glad/glad.h>

CDeferred::CDeferred(int /*width*/, int /*height*/)
	: m_shader("assets/shaders/deferred.vert", "assets/shaders/deferred.frag")
{
	// core profile 下即使不传顶点数据也必须绑一个 VAO
	glGenVertexArrays(1, &m_quadVao);
}

CDeferred::~CDeferred()
{
	if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
}

void CDeferred::Render(unsigned int albedoTex, unsigned int normalTex, unsigned int positionTex)
{
	// 全屏 Pass：不参与深度测试、不剔除、不混合
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	m_shader.use();
	m_shader.setFloat("uPBRGain", pbrGain);

	// 0~3 是阴影图、4/5 是材质贴图、6~9 是点光 cubemap，这里用 10~12
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, albedoTex);
	m_shader.setInt("gAlbedo", 10);

	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, normalTex);
	m_shader.setInt("gNormal", 11);

	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, positionTex);
	m_shader.setInt("gPosition", 12);

	// IBL：8/9 号单元放两张 cubemap，13 号放 BRDF LUT（0~12 已被别的 Pass 占用）
	m_shader.setInt("uIBLEnabled", (iblEnabled && iblIrradiance != 0) ? 1 : 0);
	m_shader.setFloat("uEnvIntensity", envIntensity);

	if (iblIrradiance != 0)
	{
		glActiveTexture(GL_TEXTURE8);
		glBindTexture(GL_TEXTURE_CUBE_MAP, iblIrradiance);
		m_shader.setInt("uIrradianceMap", 8);

		glActiveTexture(GL_TEXTURE9);
		glBindTexture(GL_TEXTURE_CUBE_MAP, iblPrefilter);
		m_shader.setInt("uPrefilterMap", 9);

		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D, iblBrdf);
		m_shader.setInt("uBrdfLUT", 13);
	}

	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}
