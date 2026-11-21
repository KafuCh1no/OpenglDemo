#include "CScene.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <iostream>
#include <string>

#include "CShader.h"
#include "CShadow.h"
#include "CShadowCube.h"
#include "CTexture.h"
#include "CModel.h"

CScene::CScene() = default;
CScene::~CScene() = default;

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
	const CMaterial& material,
	bool collide)
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

	// 先算模型空间包围盒：Build() 之后 verts 会被释放，运行时改位置就没法重算了
	glm::vec3 lmn(1e9f);
	glm::vec3 lmx(-1e9f);
	for (size_t i = 0; i + 10 < g.verts.size(); i += 11)
	{
		const glm::vec3 p(g.verts[i + 0], g.verts[i + 1], g.verts[i + 2]);
		lmn = glm::min(lmn, p);
		lmx = glm::max(lmx, p);
	}
	g.localMin = lmn;
	g.localMax = lmx;

	// 用包围盒的 8 个角点算世界 AABB（ImGui 改位置后 RefreshBox 走同样逻辑）
	if (collide && lmn.x <= lmx.x)
	{
		const glm::mat4 transform = ComposeTransform(position, rotationDeg, scale);

		glm::vec3 mn(1e9f);
		glm::vec3 mx(-1e9f);
		for (int k = 0; k < 8; ++k)
		{
			const glm::vec3 p(
				(k & 1) ? lmx.x : lmn.x,
				(k & 2) ? lmx.y : lmn.y,
				(k & 4) ? lmx.z : lmn.z);
			const glm::vec3 wp = glm::vec3(transform * glm::vec4(p, 1.0f));
			mn = glm::min(mn, wp);
			mx = glm::max(mx, wp);
		}

		g.boxIndex = (int)m_boxes.size();
		m_boxes.push_back({ mn, mx });
		std::cout << "[AddModel] AABB min=(" << mn.x << "," << mn.y << "," << mn.z
			<< ") max=(" << mx.x << "," << mx.y << "," << mx.z
			<< ") size=(" << (mx.x - mn.x) << "," << (mx.y - mn.y) << "," << (mx.z - mn.z) << ")\n";
	}

	return g;
}

void CScene::RefreshBox(Group& g)
{
	if (g.boxIndex < 0 || g.boxIndex >= (int)m_boxes.size()) return;
	if (g.localMin.x > g.localMax.x) return;

	const glm::mat4 tf = ComposeTransform(g.modelPos, g.modelRotDeg, g.modelScale);

	glm::vec3 mn(1e9f);
	glm::vec3 mx(-1e9f);
	for (int k = 0; k < 8; ++k)
	{
		const glm::vec3 p(
			(k & 1) ? g.localMax.x : g.localMin.x,
			(k & 2) ? g.localMax.y : g.localMin.y,
			(k & 4) ? g.localMax.z : g.localMin.z);
		const glm::vec3 wp = glm::vec3(tf * glm::vec4(p, 1.0f));
		mn = glm::min(mn, wp);
		mx = glm::max(mx, wp);
	}

	m_boxes[g.boxIndex] = { mn, mx };
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
	m_cubePasses.clear();

	for (int i = 0; i < count; ++i)
	{
		// 聚光：2D 深度图，每个都建（尺寸小）
		m_shadowPasses.push_back(std::make_unique<CShadow>(2048));
	}
	// 点光 cube 在 Render 里按需创建：6 面 × 1024² 很占显存，
	// 不是每盏点光都投影，没必要一次全建出来
	m_cubePasses.resize(count);
}

// ---- 阴影信息访问（供体积雾等子系统使用）----
unsigned int CScene::GetShadowMapTexture(int lightIndex) const
{
	if (lightIndex < 0 || lightIndex >= (int)m_shadowPasses.size()) return 0;
	if (!m_shadowPasses[lightIndex]) return 0;
	return m_shadowPasses[lightIndex]->GetDepthTexture();
}

glm::mat4 CScene::GetLightSpaceMatrix(int lightIndex) const
{
	if (lightIndex < 0 || lightIndex >= (int)m_shadowPasses.size()) return glm::mat4(1.0f);
	if (!m_shadowPasses[lightIndex]) return glm::mat4(1.0f);
	return m_shadowPasses[lightIndex]->GetLightSpace();
}

void CScene::UpdateFollowLights(float dt)
{
	const glm::vec3 front = GetFront();
	const glm::vec3 right = GetRight();
	const glm::vec3 up    = glm::cross(right, front);

	// 朝向指数平滑系数：dt 越大追得越多；速度越大越"硬"
	const float kDir = 1.0f - std::exp(-followDirSpeed * dt);

	for (CLight& l : m_lights)
	{
		if (!l.followCamera) continue;

		// 手持位置：相机位置 + 相机空间偏移（右/上/前）
		// 位置不做滞后，前后走动时光源立刻跟上，避免光斑前后飘
		const glm::vec3 targetPos = cameraPos
			+ right * l.followOffset.x
			+ up    * l.followOffset.y
			+ front * l.followOffset.z;

		if (!m_followInit)
		{
			// 第一帧直接对齐，避免从原点飞过来
			m_followDir  = front;
			m_followInit = true;
		}
		else
		{
			// 只有朝向滞后：转视角时手电筒会"甩"过去一点才追上
			m_followDir = glm::normalize(glm::mix(m_followDir, front, kDir));
		}

		l.position  = targetPos;
		l.direction = m_followDir;
	}
}

void CScene::BuildShadowMaps()
{
	const glm::vec3 front = GetFront();

	const int lightCount = (int)m_lights.size();

	// 所有分组几何 + 各自的世界变换，供阴影 Pass 使用
	// model 必须一起传，否则阴影会画在模型空间原点
	std::vector<CShadow::DrawItem> shadowItems;
	shadowItems.reserve(m_groups.size());
	for (auto& g : m_groups)
	{
		if (g->mesh && g->castShadow)
		{
			shadowItems.push_back({
				g->mesh.get(),
				ComposeTransform(g->modelPos, g->modelRotDeg, g->modelScale)
			});
		}
	}

	 //阴影 Pass
	 //聚光灯用 2D 深度图，点光源用 cubemap
	constexpr float SHADOW_FAR = 50.0f;
	constexpr float POINT_FAR  = 30.0f;

	m_shadowMats.assign(lightCount, glm::mat4(1.0f));
	m_pointFar.assign(lightCount, POINT_FAR);

	for (int i = 0; i < lightCount; ++i)
	{
		const CLight& l = m_lights[i];
		if (!l.castShadow || !l.enabled) continue;

		if (l.type == 1 && i < (int)m_shadowPasses.size())
		{
			// 聚光灯
			m_shadowPasses[i]->Render(shadowItems, l.position, l.direction,
				l.outerCutoffDeg, 0.1f, SHADOW_FAR);
			m_shadowMats[i] = m_shadowPasses[i]->GetLightSpace();
		}
		else if (l.type == 0 && i < (int)m_cubePasses.size())
		{
			// 点光灯
			if (!m_cubePasses[i])
				m_cubePasses[i] = std::make_unique<CShadowCube>(2048);

			m_cubePasses[i]->Render(shadowItems, l.position, 0.1f, POINT_FAR);
			m_pointFar[i] = m_cubePasses[i]->GetFarPlane();
		}
		else if (l.type == 2 && i < (int)m_shadowPasses.size())
		{
			// 平行光：用 range 当正交盒半宽
			m_shadowPasses[i]->Render(shadowItems, l.position, l.direction,
				0.0f, 0.1f, SHADOW_FAR, l.range);
			m_shadowMats[i] = m_shadowPasses[i]->GetLightSpace();
		}
	}
}

void CScene::UploadCamera(CShader& shader, const glm::mat4& projection)
{
	shader.setMat4("projection", projection);
	shader.setMat4("view", GetView());
	shader.setVec3("viewPos", cameraPos);
}

void CScene::UploadLights(CShader& shader)
{
	const int lightCount = (int)m_lights.size();

	// 把结构体数组拆成多个属性数组
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
		lPos.push_back(l.position);
		lDir.push_back(l.direction);
		lColor.push_back(l.color);
		// 平行光额外乘上闪电倍率（月光打闪），其他灯不受影响
		const float flash = (l.type == 2) ? directionalFlash : 1.0f;
		lIntensity.push_back(l.enabled ? l.intensity * flash : 0.0f);
		lCutoff.push_back(glm::cos(glm::radians(l.cutoffDeg)));
		lOuter.push_back(glm::cos(glm::radians(l.outerCutoffDeg)));
		lType.push_back(l.type);
	}

	shader.setInt("lightCount", lightCount);
	shader.setVec3Array("lightPos[0]",          lPos.data(),     lightCount);
	shader.setVec3Array("lightDirArr[0]",       lDir.data(),     lightCount);
	shader.setVec3Array("lightColorArr[0]",     lColor.data(),   lightCount);
	shader.setFloatArray("lightIntensity[0]",   lIntensity.data(), lightCount);
	shader.setFloatArray("lightCutoff[0]",      lCutoff.data(),  lightCount);
	shader.setFloatArray("lightOuterCutoff[0]", lOuter.data(),   lightCount);
	shader.setIntArray("lightType[0]",          lType.data(),    lightCount);
	shader.setMat4Array("lightSpaceMatrices[0]", m_shadowMats.data(), lightCount);
	shader.setFloatArray("pointFarPlane[0]",     m_pointFar.data(),   lightCount);

	// 阴影采样模式（ImGui 可切换）
	shader.setInt("uShadowMode", m_shadowMode);
}

void CScene::BindShadowMaps(CShader& shader)
{
	const int lightCount = (int)m_lights.size();

	// 绑定阴影贴图
	// 0壁炉 1手电筒 2洗手间灯 3月光
	for (int i = 0; i < lightCount; ++i)
	{
		const CLight& l = m_lights[i];
		if (!l.castShadow || !l.enabled) continue;

		if ((l.type == 1 || l.type == 2) && i < (int)m_shadowPasses.size())
		{
			// 聚光 / 平行光：都是 2D 深度图
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, m_shadowPasses[i]->GetDepthTexture());
			shader.setInt("shadowMap" + std::to_string(i), i);
		}
		else if (l.type == 0 && i < (int)m_cubePasses.size() && m_cubePasses[i])
		{
			// 点光
			// 0~3 阴影、4 albedo、5 normal、6+ 点光 cubemap
			const int unit = 6 + i;
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubePasses[i]->GetDepthCubemap());
			shader.setInt("shadowCube" + std::to_string(i), unit);
		}
	}
}

void CScene::DrawGroups(CShader& shader, bool hdr)
{
	// HDR 模式由主循环决定：false 时 shader 自己编码，不经过浮点缓冲
	// G-buffer 的材质 shader 没有这个 uniform，设了也是无害的空操作
	shader.setBool("uHDR", hdr);

	// 按材质分组绘制
	for (auto& g : m_groups)
	{
		if (!g->visible) continue;
		if (!g->mesh) continue;

		const CMaterial& mat = g->material;
		if (mat.HasTexture())
		{
			// 0~3 号单元被阴影贴图占了，故选 4
			mat.texture->Bind(4);
			shader.setInt("albedoMap", 4);
			shader.setBool("uHasTexture", true);
		}
		else
		{
			shader.setBool("uHasTexture", false);
		}

		if (mat.HasNormalMap())
		{
			mat.normalMap->Bind(5);      // 5 号单元，避开 0~3 阴影和 4 albedo
			shader.setInt("normalMap", 5);
			shader.setBool("uHasNormalMap", true);
			shader.setFloat("uNormalStrength", mat.normalStrength);
		}
		else
		{
			shader.setBool("uHasNormalMap", false);
		}
		shader.setVec3("tint", mat.tint);
		shader.setFloat("uScale", mat.uScale);
		// PBR 材质参数（前向 shader 没有这两个 uniform，设了是无害空操作）
		shader.setFloat("uMetallic",  mat.metallic);
		shader.setFloat("uRoughness", mat.roughness);

		// 每组的模型矩阵 + UV 模式（墙面用世界 UV，模型用自带 UV）
		shader.setMat4("model", ComposeTransform(g->modelPos, g->modelRotDeg, g->modelScale));
		shader.setBool("uUseWorldUV", g->useWorldUV);

		g->mesh->Draw();
	}
}

void CScene::Render(CShader& shader, const glm::mat4& projection, bool hdr)
{
	BuildShadowMaps();

	shader.use();
	UploadCamera(shader, projection);
	UploadLights(shader);
	BindShadowMaps(shader);

	DrawGroups(shader, hdr);
}

void CScene::RenderToGBuffer(CShader& shader, const glm::mat4& projection)
{
	// 阴影图只在这里生成一次，之后的延迟光照 Pass 直接复用
	BuildShadowMaps();

	shader.use();
	UploadCamera(shader, projection);

	DrawGroups(shader, true);
}

void CScene::SetupDeferredLighting(CShader& shader, const glm::mat4& projection)
{
	shader.use();
	UploadCamera(shader, projection);
	UploadLights(shader);
	BindShadowMaps(shader);
}

void CScene::RenderGeometryTo(CShader& shader, const glm::mat4& view,
	const glm::mat4& projection)
{
	// 阴影图沿用本帧主 Pass 的，这里只换相机
	shader.use();
	shader.setMat4("projection", projection);
	shader.setMat4("view", view);
	shader.setVec3("viewPos", cameraPos);

	DrawGroups(shader, true);
}

void CScene::SetupDeferredLightingFor(CShader& shader, const glm::mat4& view,
	const glm::mat4& projection)
{
	shader.use();
	shader.setMat4("projection", projection);
	shader.setMat4("view", view);

	UploadLights(shader);
	BindShadowMaps(shader);
}
