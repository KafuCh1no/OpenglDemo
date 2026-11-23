#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cmath>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

#include "CShader.h"
#include "CScene.h"
#include "CTexture.h"
#include "CHDR.h"
#include "CVolumetricFog.h"
#include "CFlame.h"
#include "CBloom.h"
#include "CSSAO.h"
#include "CFXAA.h"
#include "CGBuffer.h"
#include "CDeferred.h"
#include "CIBL.h"
#include "CLightning.h"
#include "CMirror.h"
#include "SceneSetup.h"

// ============ 全局场景（相机 / 几何 / 灯光 / 阴影 / 碰撞都在里面） ============
CScene gScene;

// ============ 输入状态 ============
float lastX = 640.0f, lastY = 360.0f;
bool  firstMouse = true;
float deltaTime = 0.0f, lastFrame = 0.0f;

// 手电筒开关（对应 gScene.Lights() 里的手电筒灯），默认关闭，按 F 打开
bool flashlightOn = false;

// 鼠标是否锁定：true = 转视角（光标隐藏），false = 显示光标（可点 UI）
bool mouseCaptured = true;

// 玩家碰撞半径（肩宽的一半）与眼睛到脚底的高度
constexpr float kPlayerRadius = 0.3f;
constexpr float kPlayerHeight = 1.85f;

// 手电筒在光源列表里的下标
constexpr int kFlashlightIndex = 1;

// 月光（平行光）在光源列表里的下标：体积雾用它做光柱
constexpr int kMoonlightIndex = 3;

// 蜡烛光在光源列表里的下标：初始熄灭，玩家交互后点亮
constexpr int kCandleIndex = 0;

// ---- 蜡烛交互状态 ----
bool  candleLit      = false;   // 是否已点亮
bool  candleTargeted = false;   // 准心是否对准且玩家够得着
float candleOutline  = 0.0f;    // 描边生长进度 0~1

float kInteractRange = 2.0f;    // 可交互距离（米）
float kCandleRadius  = 0.20f;   // 射线判定球半径，只用来求遮挡距离
float kAimPixels     = 40.0f;   // 准心到蜡烛投影点的像素距离阈值：越小越"必须瞄准"

// ============ 回调 ============
void framebufferSizeCallback(GLFWwindow*, int w, int h)
{
	glViewport(0, 0, w, h);
}

// 鼠标转动相机
void mouseCallback(GLFWwindow*, double xposIn, double yposIn)
{
	if (!mouseCaptured) return;          // 光标显示时不转视角

	float xpos = (float)xposIn, ypos = (float)yposIn;
	if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
	float xoffset = (xpos - lastX) * 0.5f;
	float yoffset = (lastY - ypos) * 0.5f;
	lastX = xpos; lastY = ypos;

	gScene.RotateByMouse(xoffset, yoffset);
}

// 键盘回调：只在「按下」瞬间触发一次，适合做开关类操作（Tab 切换光标）
void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
	if (action != GLFW_PRESS) return;    // 忽略 GLFW_REPEAT，长按不会反复触发

	if (key == GLFW_KEY_TAB)
	{
		mouseCaptured = !mouseCaptured;
		glfwSetInputMode(window, GLFW_CURSOR,
			mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		firstMouse = true;               // 吸收切换瞬间的光标位置跳变
	}
}

// ============ 交互检测：射线求交 ============

// 射线与球：判断"准心有没有对准某个物体"
bool RayHitSphere(const glm::vec3& origin, const glm::vec3& dir,
	const glm::vec3& center, float radius, float maxDist, float& tHit)
{
	const glm::vec3 oc = origin - center;
	const float b = glm::dot(oc, dir);
	const float c = glm::dot(oc, oc) - radius * radius;
	const float h = b * b - c;
	if (h < 0.0f) return false;

	const float sq = std::sqrt(h);
	float t = -b - sq;
	if (t < 0.0f) t = -b + sq;      // 起点已经在球内
	if (t < 0.0f || t > maxDist) return false;

	tHit = t;
	return true;
}

// 射线与 AABB：只用来判断"中间有没有东西挡住"
bool RayHitBox(const glm::vec3& origin, const glm::vec3& dir,
	const glm::vec3& mn, const glm::vec3& mx, float maxDist)
{
	float tmin = 0.0f;
	float tmax = maxDist;

	for (int i = 0; i < 3; ++i)
	{
		if (std::abs(dir[i]) < 1e-6f)
		{
			// 平行于这个轴：起点必须落在板内
			if (origin[i] < mn[i] || origin[i] > mx[i]) return false;
		}
		else
		{
			float t1 = (mn[i] - origin[i]) / dir[i];
			float t2 = (mx[i] - origin[i]) / dir[i];
			if (t1 > t2) { const float tmp = t1; t1 = t2; t2 = tmp; }

			tmin = (t1 > tmin) ? t1 : tmin;
			tmax = (t2 < tmax) ? t2 : tmax;
			if (tmin > tmax) return false;
		}
	}
	return true;
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (!mouseCaptured) return;      // Tab 放开鼠标时冻结移动

	// ① 期望位移：只取水平分量。
	// GetFront() 含俯仰角，抬头时按 W 会往天上飞；而碰撞求解只在 XZ 平面上做，
	// 就算只靠碰撞丢弃 y，卡在盒子里的那条分支也会让 y 生效。
	glm::vec3 front = gScene.GetFront();
	front.y = 0.0f;
	if (glm::length(front) < 1e-4f)          // 正上/正下看时给个默认朝向
		front = glm::vec3(0.0f, 0.0f, -1.0f);
	front = glm::normalize(front);

	glm::vec3 right = gScene.GetRight();
	right.y = 0.0f;
	right = glm::normalize(right);

	const float speed = 2.0f * deltaTime;   // 走路速度，恐怖场景不需要跑得快
	glm::vec3 delta(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += front * speed;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= front * speed;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= right * speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += right * speed;

	// ② 碰撞求解：X / Z 分轴试探，撞墙时沿墙滑动
	if (glm::length(delta) > 1e-6f)
		gScene.MoveCamera(delta, kPlayerRadius, kPlayerHeight);
}

int main()
{
	std::cout << "[main] working dir: " << std::filesystem::current_path().string() << "\n";

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(1800, 1000, "Horror Demo", nullptr, nullptr);
	if (!window)
	{
		std::cout << "[main] failed to create window\n"; glfwTerminate(); return -1;
	}

	glfwMakeContextCurrent(window);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "[main] failed to init glad\n";
		return -1;
	}
	glfwSwapInterval(0);

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	// ImGui 内置字体只有 ASCII，中文会渲染成 "?"。加载系统里的中文字体，
	// 找不到就退回默认字体（此时英文正常、中文仍是问号）。
	{
		const char* kFontCandidates[] = {
			"C:/Windows/Fonts/msyh.ttc",    // 微软雅黑
			"C:/Windows/Fonts/msyhbd.ttc",
			"C:/Windows/Fonts/simhei.ttf",  // 黑体
			"C:/Windows/Fonts/simsun.ttc",  // 宋体
		};

		// 用简体常用字集（约 2500 字）：Full 有 2 万多个字形，字体纹理会大到几十 MB
		const ImWchar* ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();

		ImFont* cnFont = nullptr;
		for (const char* path : kFontCandidates)
		{
			if (!std::filesystem::exists(path)) continue;
			cnFont = io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, ranges);
			if (cnFont) break;
		}

		if (!cnFont)
		{
			std::cout << "[ImGui] 没找到中文字体，界面中文会显示为 ?\n";
			io.Fonts->AddFontDefault();
		}

		io.FontGlobalScale = 1.15f;
	}

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// 搭建场景
	SetupScene(gScene);

	CShader shader("assets/shaders/horror.vert", "assets/shaders/horror.frag");
	// 延迟着色的几何 Pass：只写 G-buffer，不算光照
	CShader gbufferShader("assets/shaders/gbuffer.vert", "assets/shaders/gbuffer.frag");
	// 交互描边：沿法线外扩画一遍背面
	CShader outlineShader("assets/shaders/outline.vert", "assets/shaders/outline.frag");

	// HDR 离屏缓冲：场景先画进浮点纹理，再经色调映射输出到窗口
	int initW = 1800, initH = 1000;
	glfwGetFramebufferSize(window, &initW, &initH);
	CHDR hdr(initW, initH);
	CVolumetricFog fog(initW, initH);
	CFlame flame;
	CBloom bloom(initW, initH);
	CSSAO ssao(initW, initH);
	CFXAA fxaa(initW, initH);
	CGBuffer gbuf(initW, initH);
	CDeferred deferred(initW, initH);
	CMirror mirror(initW, initH);   // 洗手间镜子的平面反射


	// IBL：烘环境 cubemap + 辐照度 + 预过滤 + BRDF LUT，只需一次
	CIBL ibl;

	// 洗手间灯（2 号）已是点光，参数在 SceneSetup 里配好。
	// 这里不要再覆盖 direction / cutoffDeg，否则会被当成聚光处理。
	ibl.Generate();

	bool useDeferred = true;    // 延迟着色开关（ImGui 可切回前向）
	CLightning lightning;       // 月光闪电（只改平行光亮度，不新增光源）

	// 首帧 dt 会等于整个初始化耗时（模型加载 + IBL 烘焙十几秒），
	// 会让镜头瞬移、闪电时间轴被一次吃光，所以先把计时基准对齐到当前时间
	lastFrame = (float)glfwGetTime();

	while (!glfwWindowShouldClose(window))
	{
		float curFrame = (float)glfwGetTime();
		deltaTime = curFrame - lastFrame;
		lastFrame = curFrame;

		processInput(window);

		// 手电筒开关（只改 enabled，不覆盖 intensity，ImGui 调的值才有效）
		if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) flashlightOn = true;
		if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) flashlightOn = false;
		gScene.Lights()[kFlashlightIndex].enabled = flashlightOn;

		// 手持手电筒的滞后跟随：必须在阴影 Pass 之前更新
		gScene.UpdateFollowLights(deltaTime);

		// 闪电：更新随机时间轴，把倍率交给场景（只在 UploadLights 里作用于平行光）
		lightning.Update(deltaTime);
		gScene.directionalFlash = lightning.enabled ? lightning.MoonMultiplier() : 1.0f;

		int fbw = 0, fbh = 0;
		glfwGetFramebufferSize(window, &fbw, &fbh);

		// 窗口尺寸变化后必须重设视口，否则最终的全屏三角会画进旧视口
		if (fbw > 0 && fbh > 0)
			glViewport(0, 0, fbw, fbh);

		hdr.Resize(fbw, fbh);
		fog.Resize(fbw, fbh);
		bloom.Resize(fbw, fbh);
		ssao.Resize(fbw, fbh);
		fxaa.Resize(fbw, fbh);
		gbuf.Resize(fbw, fbh);
		mirror.Resize(fbw, fbh);

		// 镜面位置每帧同步（ImGui 里拖动镜子模型时会跟着走）
		{
			const MirrorSurface ms = BathroomMirrorSurface();
			mirror.SetSurface(ms.center, ms.normal, ms.halfWidth, ms.halfHeight);
		}

		glm::mat4 projection = glm::perspective(glm::radians(60.0f),
			(float)fbw / (float)fbh, 0.1f, 100.0f);

		// ---- 蜡烛交互：距离 + 屏幕空间瞄准 + 遮挡 ----
		{
			const glm::vec3 eye  = gScene.cameraPos;
			const glm::vec3 dir  = gScene.GetFront();
			const glm::vec3 ctr  = CandleFlamePosition() - glm::vec3(0.0f, 0.12f, 0.0f);
			const float     dist = glm::length(ctr - eye);

			candleTargeted = false;
			float tCandle = 0.0f;

			if (dist < kInteractRange)
			{
				// 用"屏幕像素距离"判定瞄准，而不是物理半径：
				// 球体在近处投影很大，稍微偏一点也会命中；换算成像素后与距离无关
				const glm::vec4 clip = projection * gScene.GetView() * glm::vec4(ctr, 1.0f);

				if (clip.w > 0.0f)
				{
					const glm::vec3 ndc = glm::vec3(clip) / clip.w;
					const float dx = ndc.x * 0.5f * (float)fbw;
					const float dy = ndc.y * 0.5f * (float)fbh;
					const float screenDist = std::sqrt(dx * dx + dy * dy);

					if (screenDist < kAimPixels &&
						RayHitSphere(eye, dir, ctr, kCandleRadius, kInteractRange, tCandle))
					{
						candleTargeted = true;

						// 中间有墙或家具挡着就不算瞄准
						for (const Box& b : gScene.Boxes())
						{
							if (RayHitBox(eye, dir, b.min, b.max, tCandle))
							{
								candleTargeted = false;
								break;
							}
						}
					}
				}
			}
		}

		// E 键：对准蜡烛时切换点燃状态
		{
			static bool ePrev = false;
			const bool  eNow  = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
			if (eNow && !ePrev && candleTargeted)
				candleLit = !candleLit;
			ePrev = eNow;
		}

		{
			// 点亮前后都显示描边：只要求瞄准即可
			const float target = candleTargeted ? 1.0f : 0.0f;
			candleOutline += (target - candleOutline) * (1.0f - std::exp(-4.5f * deltaTime));
		}

		// 光源和火焰跟着状态走
		gScene.Lights()[kCandleIndex].enabled = candleLit;
		flame.enabled = candleLit;

		if (hdr.enabled)
		{
			// 后处理要采样的深度/法线：延迟走 G-buffer，前向走 HDR 缓冲
			unsigned int depthTex  = 0;
			unsigned int normalTex = 0;

			if (useDeferred)
			{
				// ---- 几何 Pass：只写 G-buffer（阴影图也在这一步生成） ----
				gbuf.Begin();
				gScene.RenderToGBuffer(gbufferShader, projection);
				gbuf.End();

				// 环境光贴图交给光照 Pass（主画面和镜面反射共用同一套 IBL 参数）
				deferred.iblIrradiance = ibl.GetIrradianceMap();
				deferred.iblPrefilter  = ibl.GetPrefilterMap();
				deferred.iblBrdf       = ibl.GetBrdfLUT();
				deferred.iblEnabled    = ibl.enabled;
				deferred.envIntensity  = ibl.intensity;

				// ---- 平面反射：阴影图已就绪，把场景按镜像相机再画一遍 ----
				if (mirror.enabled)
				{
					// 镜子里不能看到镜子自己，反射 Pass 期间临时藏掉它
					CScene::Group* mirrorGroup = BathroomMirrorGroup();
					if (mirrorGroup) mirrorGroup->visible = false;

					mirror.RenderReflection(gScene, gbufferShader, deferred, projection,
						gScene.GetView(), gScene.cameraPos);

					if (mirrorGroup) mirrorGroup->visible = true;
				}

				// ---- 光照 Pass：逐像素读 G-buffer 算光照，写进 HDR 缓冲 ----
				// alpha 清 0：bloom 用它当自发光遮罩，只有火焰会把 alpha 抬起来
				hdr.Begin();
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_COLOR_BUFFER_BIT);

				gScene.SetupDeferredLighting(deferred.Shader(), projection);

				deferred.Render(gbuf.GetAlbedoTexture(), gbuf.GetNormalTexture(),
					gbuf.GetPositionTexture());

				// 把 G-buffer 的深度拷进来：火焰要靠它被不透明几何正确遮挡
				glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuf.GetFBO());
				glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdr.GetFBO());
				glBlitFramebuffer(0, 0, fbw, fbh, 0, 0, fbw, fbh,
					GL_DEPTH_BUFFER_BIT, GL_NEAREST);
				glBindFramebuffer(GL_FRAMEBUFFER, hdr.GetFBO());
				glEnable(GL_DEPTH_TEST);

				depthTex  = gbuf.GetDepthTexture();
				normalTex = gbuf.GetNormalTexture();
			}
			else
			{
				// HDR Pass：场景画进浮点缓冲，> 1.0 的亮度不被切掉
				hdr.Begin();
				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				// 阴影 Pass + 灯光 uniform + 按材质绘制，全在场景内部完成
				gScene.Render(shader, projection, true);

				depthTex  = hdr.GetDepthTexture();
				normalTex = hdr.GetNormalTexture();
			}

			// 镜面：把反射纹理贴到洗手间镜子上
			if (useDeferred)
				mirror.Draw(gScene, projection, gScene.GetView(), gScene.cameraPos);

			// 交互描边：进度 > 0 就画，这样淡出时也能看到轮廓收回
			// 只画背面 + 不写深度：外扩出来的壳正好露在物体外侧
			if (candleOutline > 0.002f)
			{
				if (CScene::Group* cg = CandleGroup())
				{
					glEnable(GL_DEPTH_TEST);
					glDepthMask(GL_FALSE);
					glCullFace(GL_FRONT);

					// 竖直范围：底 = 模型位置（原点在底面），顶 = 火焰根部
					const float yMin = cg->modelPos.y;
					const float yMax = CandleFlamePosition().y;

					gScene.DrawOutline(outlineShader, *cg,
						projection * gScene.GetView(), 0.006f,
						glm::vec3(0.35f, 0.02f, 0.015f),
						yMin, yMax, candleOutline);

					glCullFace(GL_BACK);
					glDepthMask(GL_TRUE);
				}
			}

			// 蜡烛火焰：半透明，放在不透明场景之后、后处理之前
			if (candleLit)
				flame.Render(projection * gScene.GetView(), CandleFlamePosition(),
					(float)glfwGetTime());

			hdr.End();

			// SSAO：读深度 + 法线，输出半分辨率遮蔽图
			ssao.Render(depthTex, normalTex, projection, gScene.GetView());

			// Bloom：读 HDR 颜色，提取亮部并模糊（1/2 -> 1/4 分辨率）
			hdr.bloomStrength = bloom.strength;
			bloom.Render(hdr.GetColorTexture());

			// 体积雾：必须排在主 Pass 之后，要读深度纹理和月光的阴影图
			unsigned int fogTex = 0;
			if (fog.enabled && fog.IsValid())
			{
				const glm::mat4 invViewProj = glm::inverse(projection * gScene.GetView());
				const CLight&   moon        = gScene.Lights()[kMoonlightIndex];

				fog.Render(depthTex, invViewProj, gScene.cameraPos,
					gScene.GetShadowMapTexture(kMoonlightIndex),
					gScene.GetLightSpaceMatrix(kMoonlightIndex),
					moon.direction, moon.color,
					moon.enabled ? moon.intensity * gScene.directionalFlash : 0.0f);

				fogTex = fog.GetTexture();
			}

			// 色调映射：写进 LDR 纹理（必须在 ImGui 之前）
			// 关掉 SSAO / Bloom 时必须传 0，否则会把上一帧的旧纹理继续贴上去
			hdr.aoStrength = ssao.strength;
			const unsigned int aoTex    = ssao.enabled  ? ssao.GetTexture()  : 0;
			const unsigned int bloomTex = bloom.enabled ? bloom.GetTexture() : 0;
			hdr.Present(fogTex, bloomTex, aoTex);

			// FXAA：最后一道，把 LDR 图像抗锯齿后铺满窗口
			fxaa.Render(hdr.GetLDRTexture());


		}
		else
		{
			// LDR 直出
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			gScene.Render(shader, projection, false);

			if (candleLit)
				flame.Render(projection * gScene.GetView(), CandleFlamePosition(),
					(float)glfwGetTime());
		}

		// ================= ImGui =================
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// ---- 准心 + 交互提示----
		if (mouseCaptured)
		{
			ImGuiViewport* vp = ImGui::GetMainViewport();
			const ImVec2 c(vp->Pos.x + vp->Size.x * 0.5f,
			               vp->Pos.y + vp->Size.y * 0.5f);
			ImDrawList* dl = ImGui::GetBackgroundDrawList();

			dl->AddCircleFilled(c, 2.0f, IM_COL32(255, 255, 255, 150), 8);
		}

		ImGui::SetNextWindowSize(ImVec2(340, 560), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
		ImGui::Begin("Horror Demo");
		ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
		ImGui::Separator();
		ImGui::Text("WASD   移动");
		ImGui::Text("F / G  手电筒开 / 关");
		ImGui::Text("Tab    切换鼠标捕获");
		ImGui::Text("左键   点亮 / 熄灭蜡烛");
		ImGui::Separator();

		// ---- 渲染管线 ----
		ImGui::Text("渲染");
		ImGui::Checkbox("HDR", &hdr.enabled);
		if (hdr.enabled)
		{
			ImGui::SameLine();
			ImGui::Checkbox("色调映射", &hdr.toneMapping);
			ImGui::SliderFloat("曝光", &hdr.exposure, 0.1f, 5.0f);
		}
		else
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(LDR 直出)");
		}
		ImGui::Checkbox("延迟着色", &useDeferred);
		ImGui::Checkbox("IBL 环境光", &ibl.enabled);
		ImGui::Checkbox("FXAA", &fxaa.enabled);
		ImGui::Separator();

		// ---- 后处理 ----
		ImGui::Text("后处理");
		ImGui::Checkbox("Bloom 泛光", &bloom.enabled);
		if (bloom.enabled)
		{
			ImGui::Checkbox("只对蜡烛发光", &bloom.onlyEmissive);
			ImGui::SliderFloat("泛光强度", &bloom.strength, 0.0f, 2.0f);
		}
		ImGui::Checkbox("SSAO 环境光遮蔽", &ssao.enabled);
		if (ssao.enabled)
			ImGui::SliderFloat("遮蔽强度", &ssao.strength, 0.0f, 2.0f);
		ImGui::Checkbox("体积雾", &fog.enabled);
		if (fog.enabled)
			ImGui::SliderFloat("雾浓度", &fog.densityBase, 0.0f, 0.30f);
		ImGui::Separator();

		// ---- 场景 ----
		ImGui::Text("场景");
		ImGui::Checkbox("镜面反射", &mirror.enabled);
		ImGui::Checkbox("闪电", &lightning.enabled);
		ImGui::Checkbox("蜡烛火焰", &flame.enabled);
		if (flame.enabled)
			ImGui::SliderFloat("火焰亮度", &flame.intensity, 0.0f, 3.0f);

		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}
