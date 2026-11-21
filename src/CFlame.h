#pragma once

#include <glm/glm.hpp>

#include "CShader.h"
#include "CMesh.h"

// 程序化火焰：一个水滴状旋转体 + 加法混合动画着色器
// 不含灯光，只负责"看得见的那团火"
class CFlame
{
public:
	CFlame();
	~CFlame() = default;

	CFlame(const CFlame&) = delete;
	CFlame& operator=(const CFlame&) = delete;

	// viewProj : projection * view
	// basePos  : 火焰根部世界坐标
	// time     : 秒
	void Render(const glm::mat4& viewProj, const glm::vec3& basePos, float time);

	// ---- 可调参数 ----
	bool  enabled   = true;
	float intensity = 5.0f;     // 亮度
	float scale     = 1.0f;     // 整体缩放
	float height    = 0.055f;   // 火焰高度（米）
	float radius    = 0.013f;   // 最大半径（米）

private:
	void BuildMesh();

	CShader m_shader;
	CMesh   m_mesh;
};
