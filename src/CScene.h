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

// 场景光源：点光 / 聚光 / 平行光，可选投影、可选跟随相机
struct CLight
{
	glm::vec3 position{ 0.0f };
	glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
	glm::vec3 color{ 1.0f };

	float intensity      = 1.0f;
	float range          = 10.0f;
	// 聚光灯内锥角（度）
	float cutoffDeg      = 30.0f;
	// 聚光灯外锥角（度）
	float outerCutoffDeg = 45.0f;

	// 0 = 点光, 1 = 聚光， 2 = 平行光
	int  type         = 1;     
	// 是否渲染深度图
	bool castShadow   = true;
	bool followCamera = false;
	// 跟随相机时的偏移（相机空间：x 右 / y 上 / z 前），用来做「手持」效果
	glm::vec3 followOffset{ 0.0f };
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
		bool                      castShadow = true;    // false = 不写入阴影图（玻璃等需要透光的面片）
		bool                      visible    = true;    // false = 本帧不画（镜面反射 Pass 里临时藏掉镜子）
		std::vector<float>        verts;
		std::vector<unsigned int> indices;
		std::unique_ptr<CMesh>    mesh;

		// 碰撞盒同步用：Build() 会清掉 verts，所以模型空间的包围盒必须提前存下来
		glm::vec3                 localMin{ 1e9f };
		glm::vec3                 localMax{ -1e9f };
		int                       boxIndex = -1;        // 在 m_boxes 里的下标，-1 = 没有碰撞盒
	};

	// 相机
	glm::vec3 cameraPos{ 1.0f, 1.85f, 9.2f };
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
	// collide = false：不生成碰撞盒（贴地/贴墙的装饰面片，例如地砖、地毯）
	Group& AddModel(const std::string& path,
		const glm::vec3& position,
		const glm::vec3& rotationDeg,
		const glm::vec3& scale,
		const CMaterial& material,
		bool collide = true);

	// 把所有分组的顶点上传到 GPU，并释放 CPU 侧数据 
	void Build();

	// 模型位置在运行时被改过之后，按新的 transform 重算它的碰撞盒
	// （ImGui 调位置时用；Box 的尺寸取 AddModel 时缓存的模型空间包围盒）
	void RefreshBox(Group& g);

	// 灯光
	CLight& AddLight(const CLight& light);
	std::vector<CLight>& Lights() { return m_lights; }

	// 手电筒跟随相机的滞后：指数平滑速度，越大跟得越紧（越小越"软"）
	float followDirSpeed = 10.0f;

	// 平行光（月光）的临时亮度倍率：闪电打闪时由主循环写进来
	float directionalFlash = 1.0f;

	// 每帧鼠标转完视角后调用一次，dt 单位秒。
	// 带 followCamera 的灯会平滑地追向目标位置/朝向，产生手持器械的延迟感。
	void UpdateFollowLights(float dt);

	// 阴影采样模式：0 = 单点，1 = PCF 3x3
	int  ShadowMode() const { return m_shadowMode; }
	void SetShadowMode(int mode) { m_shadowMode = mode; }

	// 材质分组（供 ImGui 调参）
	std::vector<std::unique_ptr<Group>>& Groups() { return m_groups; }

	// 碰撞
	// 带碰撞的移动：返回实际能到达的位置 
	void MoveCamera(const glm::vec3& delta, float radius, float height);
	const std::vector<Box>& Boxes() const { return m_boxes; }

	// 渲染
	// 内部依次执行：阴影 Pass -> 传灯光/相机 uniform -> 按材质绘制几何
	// hdr = true：输出线性值（配合 CHDR 的浮点缓冲 + 色调映射）
	// hdr = false：shader 自己编码 sRGB，直接写默认帧缓冲
	void Render(CShader& shader, const glm::mat4& projection, bool hdr = true);

	// ---- 延迟着色 ----
	// 1) 几何 Pass：只画材质属性进 G-buffer（内部会重建本帧的阴影图）
	void RenderToGBuffer(CShader& shader, const glm::mat4& projection);
	// 2) 光照 Pass 前调用：上传灯光/阴影 uniform 并绑定阴影贴图。
	//    必须在 RenderToGBuffer 之后（阴影图是它生成的），本身不再重画阴影
	void SetupDeferredLighting(CShader& shader, const glm::mat4& projection);

	// ---- 平面反射 ----
	// 只用指定的 view/projection 画一遍几何（镜面反射的 G-buffer Pass），
	// 不重画阴影图（沿用本帧主 Pass 的结果）。hdr 交给调用方决定输出格式。
	void RenderGeometryTo(CShader& shader, const glm::mat4& view,
		const glm::mat4& projection);

	// 镜面反射的延迟光照 Pass：把相机换成镜像相机后上传灯光/阴影 uniform。
	// 必须在本帧 RenderToGBuffer 之后调用，否则阴影图还没生成。
	// 镜像相机等非主视角用：必须把视线原点一起传进去，
	// 否则 viewPos 还是主相机位置，NdotV / 菲涅尔 / 高光都会算错
	void SetupDeferredLightingFor(CShader& shader, const glm::mat4& view,
		const glm::mat4& projection, const glm::vec3& eyePos);

	// 只上传光源参数（不含相机/阴影），给自定义前向 pass 用，比如镜面高光
	void UploadLightingUniforms(CShader& shader);

	// 描边：把单个分组的网格沿法线外扩画一遍（交互高亮用）
	// 面剔除/深度状态由调用方设置：通常是 glCullFace(GL_FRONT) + glDepthMask(GL_FALSE)
	// yMin/yMax 是物体世界高度范围，progress 控制从下往上的生长（1 = 完整显示）
	void DrawOutline(CShader& shader, Group& g, const glm::mat4& viewProj,
		float thickness, const glm::vec3& color,
		float yMin, float yMax, float progress);

	// 预先创建n个阴影 Pass
	void PrepareShadows(int count = 3);

	// 某个光源的阴影信息（体积雾等子系统读取）
	// 该索引没有可用的 2D 阴影图时返回 0 / 单位矩阵
	unsigned int GetShadowMapTexture(int lightIndex) const;
	glm::mat4    GetLightSpaceMatrix(int lightIndex) const;

private:
	// 渲染拆成几步，前向和延迟共用
	void BuildShadowMaps();       // 重画本帧所有阴影图
	void UploadCamera(CShader& shader, const glm::mat4& projection);
	void UploadLights(CShader& shader);
	void BindShadowMaps(CShader& shader);
	void DrawGroups(CShader& shader, bool hdr);

	std::vector<glm::mat4>                    m_shadowMats;   // 本帧各光源的 lightSpace 矩阵
	std::vector<float>                        m_pointFar;     // 本帧各点光的 farPlane

	// 手电筒平滑跟随用的状态（首次调用时直接对齐，之后指数插值）
	glm::vec3                                 m_followDir{ 0.0f, 0.0f, -1.0f };
	bool                                      m_followInit = false;

	std::vector<std::unique_ptr<Group>>       m_groups;
	std::vector<CLight>                       m_lights;
	std::vector<std::unique_ptr<CShadow>>     m_shadowPasses;   // 聚光灯：2D 深度图
	std::vector<std::unique_ptr<CShadowCube>> m_cubePasses;     // 点光源：cubemap
	std::vector<Box>                          m_boxes;

	int                                       m_shadowMode = 1;   // 1 = PCF 3x3
};
