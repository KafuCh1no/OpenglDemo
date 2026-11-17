#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "CShader.h"
#include "CShadow.h"        // 复用 CShadow::DrawItem，避免两套结构

class CMesh;

// 点光源阴影（Shadow Cube）
// 点光向四面八方发光，一张 2D 图装不下，所以用 cubemap：
// 每帧从光源位置朝 6 个方向各渲染一次，把 6 个面拼成一个立方体贴图。
//
// 和聚光灯的区别：
//   1. 渲染 6 次（每面一次），不是 1 次
//   2. 深度里存的是「到光源的线性距离 / farPlane」，不是 NDC 深度
//   3. 主 Pass 用 samplerCube + 方向向量采样，不需要 lightSpaceMatrix
class CShadowCube
{
public:
	explicit CShadowCube(int size = 512);
	~CShadowCube();

	CShadowCube(const CShadowCube&) = delete;
	CShadowCube& operator=(const CShadowCube&) = delete;

	// 深度 Pass：6 个方向各画一遍
	void Render(const std::vector<CShadow::DrawItem>& items,
		const glm::vec3& lightPos,
		float nearZ,
		float farZ);

	unsigned int GetDepthCubemap() const { return m_depthCube; }
	float GetFarPlane() const { return m_far; }
	int GetSize() const { return m_size; }

private:
	void CreateDepthCubemap();

	int m_size = 512;

	unsigned int m_fbo = 0;
	unsigned int m_depthCube = 0;

	// 主 Pass 比较距离时要除同一个 farPlane，所以存下来
	float m_far = 30.0f;

	CShader m_depthShader;
};
