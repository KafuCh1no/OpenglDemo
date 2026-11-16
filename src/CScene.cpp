#include "CScene.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>
#include <string>

#include "CShader.h"
#include "CShadow.h"
#include "CTexture.h"
#include "CModel.h"

CScene::CScene() = default;
CScene::~CScene() = default;

namespace
{
	// 把 AABB 转成顶点/索引追加进数组：每顶点 11 float（pos3 | normal3 | color3 | uv2）
	void AppendBoxMesh(std::vector<float>& verts, std::vector<unsigned int>& indices,
		float x0, float y0, float z0,
		float x1, float y1, float z1,
		const glm::vec3& color)
	{
		unsigned int base = (unsigned int)(verts.size() / 11);

		struct Face { glm::vec3 n; glm::vec3 v[4]; };
		Face faces[6] = {
			{ { 0, 0, 1}, {{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}} },  // +Z
			{ { 0, 0,-1}, {{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}} },  // -Z
			{ { 1, 0, 0}, {{x1,y0,z1},{x1,y0,z0},{x1,y1,z0},{x1,y1,z1}} },  // +X
			{{-1, 0, 0}, {{x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0}} },   // -X
			{ { 0, 1, 0}, {{x0,y1,z1},{x1,y1,z1},{x1,y1,z0},{x0,y1,z0}} },  // +Y
			{ { 0,-1, 0}, {{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1}} },  // -Y
		};

		// 每面 4 个角的 UV：左下、右下、右上、左上（平铺交给 shader 的 uScale）
		static const glm::vec2 kQuadUV[4] = { {0.0f,0.0f}, {1.0f,0.0f}, {1.0f,1.0f}, {0.0f,1.0f} };

		for (auto& f : faces)
		{
			for (int i = 0; i < 4; ++i)
			{
				const glm::vec3& c = f.v[i];
				verts.push_back(c.x); verts.push_back(c.y); verts.push_back(c.z);
				verts.push_back(f.n.x); verts.push_back(f.n.y); verts.push_back(f.n.z);
				verts.push_back(color.r); verts.push_back(color.g); verts.push_back(color.b);
				verts.push_back(kQuadUV[i].x); verts.push_back(kQuadUV[i].y);
			}
			indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
			base += 4;
		}
	}

	// 位置 / 欧拉角（度）/ 缩放 -> 模型矩阵
	glm::mat4 ComposeTransform(const glm::vec3& pos, const glm::vec3& rotDeg, const glm::vec3& scale)
	{
		glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
		m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
		m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
		m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
		m = glm::scale(m, scale);
		return m;
	}
}

// 相机
glm::vec3 CScene::GetFront() const
{
	const float ry = glm::radians(yaw);
	const float rp = glm::radians(pitch);
	return glm::normalize(glm::vec3(
		std::cos(ry) * std::cos(rp),
		std::sin(rp),
		std::sin(ry) * std::cos(rp)));
}

glm::vec3 CScene::GetRight() const
{
	return glm::normalize(glm::cross(GetFront(), cameraUp));
}

glm::mat4 CScene::GetView() const
{
	return glm::lookAt(cameraPos, cameraPos + GetFront(), cameraUp);
}

void CScene::RotateByMouse(float xoffsetPixels, float yoffsetPixels)
{
	yaw += xoffsetPixels * mouseSensitivity;
	pitch += yoffsetPixels * mouseSensitivity;
	pitch = glm::clamp(pitch, minPitch, maxPitch);
}

// 几何
CScene::Group& CScene::NewGroup(const CMaterial& material)
{
	m_groups.push_back(std::make_unique<Group>());
	m_groups.back()->material = material;
	return *m_groups.back();
}

void CScene::AddBox(Group& g,
	float x0, float y0, float z0,
	float x1, float y1, float z1,
	const glm::vec3& color)
{
	AppendBoxMesh(g.verts, g.indices, x0, y0, z0, x1, y1, z1, color);
	m_boxes.push_back({ glm::vec3(x0, y0, z0), glm::vec3(x1, y1, z1) });
}

// 将CPU的顶点数据搬到GPU中
CScene::Group& CScene::AddModel(const std::string& path,
	const glm::vec3& position,
	const glm::vec3& rotationDeg,
	const glm::vec3& scale,
	const CMaterial& material)
{
	m_groups.push_back(std::make_unique<Group>());
	Group& g = *m_groups.back();

	g.material    = material;
	g.modelPos    = position;
	g.modelRotDeg = rotationDeg;
	g.modelScale  = scale;
	g.useWorldUV  = false;   // 模型用自带 UV

	if (!CModel::Load(path, g.verts, g.indices))
	{
		g.verts.clear();
		g.indices.clear();
		return g;
	}

	const glm::mat4 transform = ComposeTransform(position, rotationDeg, scale);

	// 按变换后的顶点算 AABB，直接用作碰撞盒
	glm::vec3 mn(1e9f);
	glm::vec3 mx(-1e9f);
	for (size_t i = 0; i + 10 < g.verts.size(); i += 11)
	{
		const glm::vec3 p(g.verts[i + 0], g.verts[i + 1], g.verts[i + 2]);
		const glm::vec3 wp = glm::vec3(transform * glm::vec4(p, 1.0f));
		mn = glm::min(mn, wp);
		mx = glm::max(mx, wp);
	}

	if (mn.x <= mx.x)
	{
		m_boxes.push_back({ mn, mx });
		std::cout << "[AddModel] AABB min=(" << mn.x << "," << mn.y << "," << mn.z
			<< ") max=(" << mx.x << "," << mx.y << "," << mx.z
			<< ") size=(" << (mx.x - mn.x) << "," << (mx.y - mn.y) << "," << (mx.z - mn.z) << ")\n";
	}

	return g;
}

void CScene::Build()
{
	for (auto& g : m_groups)
	{
		// 组中没东西 直接跳过
		if (g->verts.empty()) continue;

		// 创建 GL 资源（VAO/VBO/EBO）
		g->mesh = std::make_unique<CMesh>();
		// 把顶点和索引拷进显存
		g->mesh->Upload(g->verts, g->indices);

		// 释放CPU的内存
		g->verts.clear();
		g->verts.shrink_to_fit();
		g->indices.clear();
		g->indices.shrink_to_fit();
	}
}

// 灯光

CLight& CScene::AddLight(const CLight& light)
{
	m_lights.push_back(light);
	return m_lights.back();
}

// 碰撞

void CScene::MoveCamera(const glm::vec3& delta, float radius, float height)
{
	cameraPos = CCollision::MoveWithCollision(cameraPos, delta, radius, height, m_boxes);
}

// 渲染
// 预先创建阴影Pass
void CScene::PrepareShadows(int count)
{
	m_shadowPasses.clear();
	for (int i = 0; i < count; ++i)
	{
		m_shadowPasses.push_back(std::make_unique<CShadow>(1024));
	}
}

void CScene::Render(CShader& shader, const glm::mat4& projection)
{
	const glm::vec3 front = GetFront();

	// 手电筒
	for (CLight& l : m_lights)
	{
		if (l.followCamera)
		{
			l.Position = cameraPos;
			l.direction = front;
		}
	}

	const int lightCount = (int)m_lights.size();

	// 所有分组几何，供阴影 Pass 使用
	std::vector<const CMesh*> meshes;
	meshes.reserve(m_groups.size());
	for (auto& g : m_groups)
	{
		if (g->mesh)
		{
			meshes.push_back(g->mesh.get());
		}
	}

	 //阴影 Pass
	 //准备矩阵数组
	std::vector<glm::mat4> shadowMats(lightCount, glm::mat4(1.0f));

	for (int i = 0; i < lightCount && i < (int)m_shadowPasses.size(); ++i)
	{
		const CLight& l = m_lights[i];
		if (l.type != 1 || !l.castShadow || !l.enabled) continue;

		// 深度Pass
		m_shadowPasses[i]->Render(meshes, l.Position, l.direction, l.outerCutoffDeg, 0.1f, 50.0f);
		// 记录矩阵
		shadowMats[i] = m_shadowPasses[i]->GetLightSpace();
	}

	// 把结构体数组拆绳多个属性数组
	std::vector<glm::vec3> lPos, lDir, lColor;
	std::vector<float>     lIntensity, lCutoff, lOuter;
	std::vector<int>       lType;

	lPos.reserve(lightCount);
	lDir.reserve(lightCount);
	lColor.reserve(lightCount);
	lIntensity.reserve(lightCount);
	lCutoff.reserve(lightCount);
	lOuter.reserve(lightCount);
	lType.reserve(lightCount);

	for (const CLight& l : m_lights)
	{
		lPos.push_back(l.Position);
		lDir.push_back(l.direction);
		lColor.push_back(l.color);
		lIntensity.push_back(l.enabled ? l.intensity : 0.0f);
		lCutoff.push_back(glm::cos(glm::radians(l.cutoffDeg)));
		lOuter.push_back(glm::cos(glm::radians(l.outerCutoffDeg)));
		lType.push_back(l.type);
	}

	// 设置uniform数值
	shader.use();
	shader.setMat4("projection", projection);
	shader.setMat4("view", GetView());
	shader.setVec3("viewPos", cameraPos);

	shader.setInt("lightCount", lightCount);
	shader.setVec3Array("lightPos[0]",          lPos.data(),     lightCount);
	shader.setVec3Array("lightDirArr[0]",       lDir.data(),     lightCount);
	shader.setVec3Array("lightColorArr[0]",     lColor.data(),   lightCount);
	shader.setFloatArray("lightIntensity[0]",   lIntensity.data(), lightCount);
	shader.setFloatArray("lightCutoff[0]",      lCutoff.data(),  lightCount);
	shader.setFloatArray("lightOuterCutoff[0]", lOuter.data(),   lightCount);
	shader.setIntArray("lightType[0]",          lType.data(),    lightCount);
	shader.setMat4Array("lightSpaceMatrices[0]", shadowMats.data(), lightCount);

	// 绑定阴影贴图
	for (int i = 0; i < lightCount && i < (int)m_shadowPasses.size(); ++i)
	{
		const CLight& l = m_lights[i];
		if (l.type != 1 || !l.castShadow || !l.enabled) continue;

		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_shadowPasses[i]->GetDepthTexture());
		shader.setInt("shadowMap" + std::to_string(i), i);
	}

	// 按材质分组绘制
	for (auto& g : m_groups)
	{
		if (!g->mesh) continue;

		const CMaterial& mat = g->material;
		if (mat.HasTexture())
		{
			// 0 1 2号纹理单元被阴影贴图占了 故选3
			mat.texture->Bind(3);
			shader.setInt("albedoMap", 3);
			shader.setBool("uHasTexture", true);
		}
		else
		{
			shader.setBool("uHasTexture", false);
		}

		if (mat.HasNormalMap())
		{
			mat.normalMap->Bind(4);      // 4 号单元，避开 0/1/2 阴影和 3 albedo
			shader.setInt("normalMap", 4);
			shader.setBool("uHasNormalMap", true);
			shader.setFloat("uNormalStrength", mat.normalStrength);
		}
		else
		{
			shader.setBool("uHasNormalMap", false);
		}
		shader.setVec3("tint", mat.tint);
		shader.setFloat("uScale", mat.uScale);

		// 每组的模型矩阵 + UV 模式（墙面用世界 UV，模型用自带 UV）
		shader.setMat4("model", ComposeTransform(g->modelPos, g->modelRotDeg, g->modelScale));
		shader.setBool("uUseWorldUV", g->useWorldUV);

		g->mesh->Draw();
	}
}
