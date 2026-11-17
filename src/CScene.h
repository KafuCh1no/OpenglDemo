#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

#include "CMesh.h"
#include "CMaterial.h"
#include "CCollision.h"

class CShader;
class CShadow;
class CShadowCube;

// 场景光源：点光 / 聚光，可选投影、可选跟随相机
struct CLight
{
	glm::vec3 Position{ 0.0f };
	glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
	glm::vec3 color{ 1.0f };

	float intensity      = 1.0f;
	float range          = 10.0f;
	float cutoffDeg      = 30.0f;   // 内锥角（度）
	float outerCutoffDeg = 45.0f;   // 外锥角（度）

	// 0 = 点光, 1 = 聚光
	int  type         = 1;      
	bool castShadow   = true;
	bool followCamera = false;
	bool enabled      = true;
};

// 场景：统一管理相机、几何分组、光源、阴影 Pass、碰撞盒。
// 几何按材质分组（Group），每组一次 draw call；
class CScene
{
public:
	CScene();
	~CScene();

	CScene(const CScene&) = delete;
	CScene& operator=(const CScene&) = delete;

	// 一组 = 同一种材质 + 它自己的几何 
	struct Group
	{
		CMaterial                 material;
		glm::vec3                 modelPos{ 0.0f };     // 位置
		glm::vec3                 modelRotDeg{ 0.0f };  // 欧拉角（度）
		glm::vec3                 modelScale{ 1.0f };   // 缩放
		bool                      useWorldUV = true;    // true = 世界 UV（墙面），false = 模型自带 UV
		std::vector<float>        verts;
		std::vector<unsigned int> indices;
		std::unique_ptr<CMesh>    mesh;

		// 碰撞盒同步用：Build() 会清掉 verts，所以模型空间的包围盒必须提前存下来
		glm::vec3                 localMin{ 1e9f };
		glm::vec3                 localMax{ -1e9f };
		int                       boxIndex = -1;        // 在 m_boxes 里的下标，-1 = 没有碰撞盒
	};

	// 相机
	glm::vec3 cameraPos{ 1.0f, 1.7f, 9.2f };
	glm::vec3 cameraUp{ 0.0f, 1.0f, 0.0f };
	float yaw   = -90.0f;
	float pitch = 0.0f;
	float mouseSensitivity = 0.1f;
	float minPitch = -89.0f;
	float maxPitch = 89.0f;

	glm::vec3 GetFront() const;
	glm::vec3 GetRight() const;
	glm::mat4 GetView() const;

	// 鼠标像素增量 -> 旋转视角（内部会 clamp pitch） 
	void RotateByMouse(float xoffsetPixels, float yoffsetPixels);

	// 几何
	Group& NewGroup(const CMaterial& material);

	void AddBox(Group& g,
		float x0, float y0, float z0,
		float x1, float y1, float z1,
		const glm::vec3& color);

	// 加载 .obj 模型：位置/旋转/缩放分开传，方便 ImGui 直接调
	// 纹理走模型自带 UV；加载成功后自动按世界坐标算一个 AABB 加进碰撞盒
	Group& AddModel(const std::string& path,
		const glm::vec3& position,
		const glm::vec3& rotationDeg,
		const glm::vec3& scale,
		const CMaterial& material);

	// 把所有分组的顶点上传到 GPU，并释放 CPU 侧数据 
	void Build();

	// 模型位置在运行时被改过之后，按新的 transform 重算它的碰撞盒
	// （ImGui 调位置时用；Box 的尺寸取 AddModel 时缓存的模型空间包围盒）
	void RefreshBox(Group& g);

	// 灯光
	CLight& AddLight(const CLight& light);
	std::vector<CLight>& Lights() { return m_lights; }

	// 材质分组（供 ImGui 调参）
	std::vector<std::unique_ptr<Group>>& Groups() { return m_groups; }

	// 碰撞
	// 带碰撞的移动：返回实际能到达的位置 
	void MoveCamera(const glm::vec3& delta, float radius, float height);
	const std::vector<Box>& Boxes() const { return m_boxes; }

	// 渲染
	// 内部依次执行：阴影 Pass -> 传灯光/相机 uniform -> 按材质绘制几何 
	void Render(CShader& shader, const glm::mat4& projection);

	// 预先创建n个阴影 Pass
	void PrepareShadows(int count = 3);

private:
	std::vector<std::unique_ptr<Group>>       m_groups;
	std::vector<CLight>                       m_lights;
	std::vector<std::unique_ptr<CShadow>>     m_shadowPasses;   // 聚光灯：2D 深度图
	std::vector<std::unique_ptr<CShadowCube>> m_cubePasses;     // 点光源：cubemap
	std::vector<Box>                          m_boxes;
};
