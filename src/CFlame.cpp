#include "CFlame.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	// 绕一圈的段数
	constexpr int   kSegs  = 24;
	// 从底到顶的圈数
	constexpr int   kRings = 20;
	constexpr float kPI    = 3.14159265f;

	// 底部收拢、约 1/3 处最鼓、顶部收成尖
	float ProfileRadius(float y)
	{
		float s = std::sin(kPI * std::pow(y, 0.75f));
		return std::pow(std::max(s, 0.0f), 0.9f);
	}
}

CFlame::CFlame()
	: m_shader("assets/shaders/flame.vert", "assets/shaders/flame.frag")
{
	BuildMesh();
}

void CFlame::BuildMesh()
{
	std::vector<float> verts;
	std::vector<unsigned int> indices;

	verts.reserve((kRings + 1) * (kSegs + 1) * 11);

	for (int j = 0; j <= kRings; ++j)
	{
		const float y  = (float)j / (float)kRings;
		const float rr = ProfileRadius(y);

		for (int i = 0; i <= kSegs; ++i)
		{
			const float a  = 2.0f * kPI * (float)i / (float)kSegs;
			const float cx = std::cos(a);
			const float sz = std::sin(a);

			// 单位尺寸，实际大小由 shader 里的 uRadius / uHeight 控制
			const float px = cx * rr;
			const float pz = sz * rr;

			// 法线：径向外扩，略向上倾
			const float nx = cx;
			const float ny = 0.35f;
			const float nz = sz;
			const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);

			verts.push_back(px);
			verts.push_back(y);
			verts.push_back(pz);

			verts.push_back(nx / nl);
			verts.push_back(ny / nl);
			verts.push_back(nz / nl);

			verts.push_back(1.0f);
			verts.push_back(1.0f);
			verts.push_back(1.0f);

			verts.push_back((float)i / (float)kSegs);   // u 绕圈
			verts.push_back(y);                          // v 高度
		}
	}

	for (int j = 0; j < kRings; ++j)
	{
		for (int i = 0; i < kSegs; ++i)
		{
			const unsigned int a = (unsigned int)(j * (kSegs + 1) + i);
			const unsigned int b = a + 1;
			const unsigned int c = a + (unsigned int)(kSegs + 1);
			const unsigned int d = c + 1;

			indices.push_back(a); indices.push_back(c); indices.push_back(b);
			indices.push_back(b); indices.push_back(c); indices.push_back(d);
		}
	}

	m_mesh.Upload(verts, indices);
}

void CFlame::Render(const glm::mat4& viewProj, const glm::vec3& basePos, float time)
{
	if (!enabled) return;

	// 半透明：加法混合、不写深度、不剔除（从任何角度看都是满的）
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	m_shader.use();
	m_shader.setMat4("uViewProj", viewProj);
	m_shader.setVec3("uBasePos", basePos);
	m_shader.setFloat("uTime", time);
	m_shader.setFloat("uIntensity", intensity);
	m_shader.setFloat("uScale", scale);
	m_shader.setFloat("uHeight", height);
	m_shader.setFloat("uRadius", radius);

	m_mesh.Draw();

	// 复原，别影响后面的 pass
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
}
