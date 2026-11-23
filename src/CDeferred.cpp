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

	// IBL：14/15 号单元放两张 cubemap，13 号放 BRDF LUT。
	// 不能用 8/9：CScene 给点光源绑 cubemap 阴影用的是 6+i，
	// 洗手间灯是 2 号 → 占 8 号单元，和 irradiance 会互相覆盖，
	// 结果是那盏灯的阴影采样拿到一张环境图（阴影全错）。
	m_shader.setInt("uIBLEnabled", (iblEnabled && iblIrradiance != 0) ? 1 : 0);
	m_shader.setFloat("uEnvIntensity", envIntensity);

	if (iblIrradiance != 0)
	{
		glActiveTexture(GL_TEXTURE14);
		glBindTexture(GL_TEXTURE_CUBE_MAP, iblIrradiance);
		m_shader.setInt("uIrradianceMap", 14);

		glActiveTexture(GL_TEXTURE15);
		glBindTexture(GL_TEXTURE_CUBE_MAP, iblPrefilter);
		m_shader.setInt("uPrefilterMap", 15);

		glActiveTexture(GL_TEXTURE13);
		glBindTexture(GL_TEXTURE_2D, iblBrdf);
		m_shader.setInt("uBrdfLUT", 13);
	}

	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);
}
