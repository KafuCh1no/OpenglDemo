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

// 手电筒开关（对应 gScene.Lights() 里的手电筒灯）
bool flashlightOn = true;

// 鼠标是否锁定：true = 转视角（光标隐藏），false = 显示光标（可点 UI）
bool mouseCaptured = true;

// 玩家碰撞半径（肩宽的一半）与眼睛到脚底的高度
constexpr float kPlayerRadius = 0.3f;
constexpr float kPlayerHeight = 1.85f;

// 手电筒在光源列表里的下标
constexpr int kFlashlightIndex = 1;

// 月光（平行光）在光源列表里的下标：体积雾用它做光柱
constexpr int kMoonlightIndex = 3;

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

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (!mouseCaptured) return;      // Tab 放开鼠标时冻结移动

	// ① 先算出「期望位移」
	const float speed = 6.0f * deltaTime;
	const glm::vec3 front = gScene.GetFront();
	const glm::vec3 right = gScene.GetRight();
	glm::vec3 delta(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += front * speed;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= front * speed;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= right * speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += right * speed;

	// ② 再交给场景的碰撞求解，决定实际能走多少
	if (glm::length(delta) > 1e-6f)
		// 调试用，正式要把注释解除
		//gScene.MoveCamera(delta, kPlayerRadius, kPlayerHeight);
		gScene.cameraPos += delta;
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
	ImGui::GetIO().FontGlobalScale = 1.7f;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// 搭建场景
	SetupScene(gScene);
	auto& models = SceneModels();          // ImGui 面板用

	CShader shader("assets/shaders/horror.vert", "assets/shaders/horror.frag");
	// 延迟着色的几何 Pass：只写 G-buffer，不算光照
	CShader gbufferShader("assets/shaders/gbuffer.vert", "assets/shaders/gbuffer.frag");

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
	gScene.Lights()[2].direction = glm::normalize(glm::vec3(0.0f, -0.30f, -1.0f));
	gScene.Lights()[2].cutoffDeg = 18.0f;
	gScene.Lights()[2].outerCutoffDeg = 55.0f;
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
				hdr.Begin();
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				// 阴影 Pass + 灯光 uniform + 按材质绘制，全在场景内部完成
				gScene.Render(shader, projection, true);

				depthTex  = hdr.GetDepthTexture();
				normalTex = hdr.GetNormalTexture();
			}

			// 镜面：把反射纹理贴到洗手间镜子上（深度测试用 G-buffer 的深度）
			// 只有延迟路径做了反射 Pass，前向路径这里跳过
			if (useDeferred)
				mirror.Draw(projection, gScene.GetView());

			// 蜡烛火焰：半透明，放在不透明场景之后、后处理之前
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

			flame.Render(projection * gScene.GetView(), CandleFlamePosition(),
				(float)glfwGetTime());
		}

		// ================= ImGui =================
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(460, 720), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
		ImGui::Begin("ImGui");
		ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
		ImGui::Separator();

		// HDR 总开关：关掉后完全绕过浮点缓冲，等同加 HDR 之前的渲染
		ImGui::Checkbox("HDR", &hdr.enabled);
		if (hdr.enabled)
		{
			ImGui::SameLine();
			ImGui::Checkbox("tone mapping", &hdr.toneMapping);
			ImGui::SliderFloat("exposure", &hdr.exposure, 0.1f, 5.0f);
		}
		else
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(LDR 直出，8 bit)");
		}
		ImGui::Separator();

		// 体积雾（平行光，月光穿过窗洞形成光柱）
		ImGui::Checkbox("volumetric fog", &fog.enabled);
		ImGui::Checkbox("SSAO", &ssao.enabled);
		if (ssao.enabled)
		{
			ImGui::SliderFloat("ssao radius", &ssao.radius, 0.1f, 4.0f);
			ImGui::SliderFloat("ssao bias", &ssao.bias, 0.0f, 0.1f);
			ImGui::SliderFloat("ssao strength", &ssao.strength, 0.0f, 2.0f);
			ImGui::SliderFloat("ssao power", &ssao.power, 1.0f, 4.0f);
		}
		ImGui::Checkbox("bloom", &bloom.enabled);
		if (bloom.enabled)
		{
			ImGui::SliderFloat("bloom threshold", &bloom.threshold, 0.0f, 4.0f);
			ImGui::SliderFloat("bloom strength", &bloom.strength, 0.0f, 2.0f);
			ImGui::SliderFloat("bloom radius", &bloom.radius, 0.2f, 4.0f);
		}
		ImGui::Checkbox("FXAA", &fxaa.enabled);
		ImGui::Checkbox("deferred shading", &useDeferred);
		ImGui::SliderFloat("pbr gain", &deferred.pbrGain, 1.0f, 6.0f);
		ImGui::Checkbox("IBL", &ibl.enabled);
		ImGui::SliderFloat("env intensity", &ibl.intensity, 0.0f, 3.0f);
		ImGui::Checkbox("mirror", &mirror.enabled);
		ImGui::SliderFloat("mirror tint", &mirror.tint, 0.0f, 1.5f);

		// 闪电（乘在月光亮度上，不新增光源）
		ImGui::Checkbox("lightning", &lightning.enabled);
		if (lightning.enabled)
		{
			ImGui::SliderFloat("flash gain", &lightning.gain, 1.0f, 30.0f);
			ImGui::SliderFloat("flash min gap", &lightning.minInterval, 0.5f, 20.0f);
			ImGui::SliderFloat("flash max gap", &lightning.maxInterval, 1.0f, 40.0f);
			if (ImGui::Button("flash now"))
				lightning.Trigger();
		}

		ImGui::Checkbox("candle flame", &flame.enabled);
		if (flame.enabled)
		{
			ImGui::SliderFloat("flame intensity", &flame.intensity, 0.0f, 3.0f);
			ImGui::SliderFloat("flame scale", &flame.scale, 0.2f, 3.0f);
		}
		if (fog.enabled)
		{
			if (!hdr.enabled)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(需要 HDR 开启)");
			}
			ImGui::SliderFloat("fog density",  &fog.densityBase,   0.0f, 0.30f);
			ImGui::SliderFloat("fog falloff",  &fog.heightFalloff, 0.0f, 2.0f);
			ImGui::SliderFloat("fog distance", &fog.maxDistance,   5.0f, 60.0f);
			ImGui::SliderFloat("fog phase",    &fog.phaseG,       -0.9f, 0.95f);
		}
		ImGui::Separator();

		auto& lights = gScene.Lights();
		for (int i = 0; i < (int)lights.size(); ++i)
		{
			ImGui::PushID(i);
			CLight& l = lights[i];
			const char* typeName = (l.type == 1) ? "spot" : (l.type == 2 ? "directional" : "point");
			ImGui::Text("Light %d (%s)", i, typeName);
			ImGui::Checkbox("enabled", &l.enabled);
			if (!l.followCamera)
			{
				ImGui::DragFloat3("position", &l.position.x, 0.05f);
				ImGui::DragFloat3("direction", &l.direction.x, 0.02f);
			}
			ImGui::SliderFloat("intensity", &l.intensity, 0.0f, 20.0f);
			if (l.type == 1)
			{
				ImGui::SliderFloat("cutoff", &l.cutoffDeg, 1.0f, 80.0f);
				ImGui::SliderFloat("outerCutoff", &l.outerCutoffDeg, 1.0f, 89.0f);
			}
			if (l.type == 2)
			{
				ImGui::SliderFloat("range", &l.range, 0.5f, 50.0f);
			}
			ImGui::Checkbox("castShadow", &l.castShadow);
			ImGui::ColorEdit3("color", &l.color.x);

			// 手持跟随的滞后程度（只影响朝向，位置是即时跟随）
			if (l.followCamera)
			{
				ImGui::SliderFloat("hold dir speed", &gScene.followDirSpeed, 0.5f, 30.0f);
			}
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::Separator();
		int shadowMode = gScene.ShadowMode();
		const char* kShadowModes[] = { "single tap", "PCF 3x3", "PCSS" };
		if (ImGui::Combo("shadow filter", &shadowMode, kShadowModes, 3))
			gScene.SetShadowMode(shadowMode);

		ImGui::Separator();
		ImGui::Text("Materials");
		auto& groups = gScene.Groups();
		for (int i = 0; i < (int)groups.size(); ++i)
		{
			CMaterial& mat = groups[i]->material;
			ImGui::PushID(1000 + i);
			ImGui::Text("Group %d (%s)", i,
				mat.HasNormalMap() ? "albedo+normal" : (mat.HasTexture() ? "albedo" : "solid"));
			ImGui::SliderFloat("uScale /m", &mat.uScale, 0.02f, 3.0f);
			ImGui::SliderFloat("normalStrength", &mat.normalStrength, 0.0f, 3.0f);
			ImGui::SliderFloat("metallic", &mat.metallic, 0.0f, 1.0f);
			ImGui::SliderFloat("roughness", &mat.roughness, 0.04f, 1.0f);
			ImGui::ColorEdit3("tint", &mat.tint.x);
			ImGui::Separator();
			ImGui::PopID();
		}

		// ---------------- 模型位置 ----------------
		ImGui::Separator();
		ImGui::Text("Models");

		// 默认关闭：拖动后的值会一直保留，不会被代码里的初值覆盖
		// 勾上则每帧写回初值（用于对比 / 复位调试）
		static bool autoReset = false;
		ImGui::Checkbox("auto reset every frame", &autoReset);
		ImGui::SameLine();
		const bool clickedReset = ImGui::Button("Reset All");

		if (clickedReset || autoReset)
		{
			for (auto& m : models)
			{
				m.group->modelPos    = m.initPos;
				m.group->modelRotDeg = m.initRot;
				m.group->modelScale  = m.initScale;
				gScene.RefreshBox(*m.group);
			}
		}

		for (int i = 0; i < (int)models.size(); ++i)
		{
			ModelHandle& m = models[i];
			ImGui::PushID(2000 + i);
			ImGui::Text("%s", m.name.c_str());

			bool changed = false;
			changed |= ImGui::DragFloat3("pos", &m.group->modelPos.x, 0.02f);
			changed |= ImGui::DragFloat3("rot", &m.group->modelRotDeg.x, 0.5f);

			float s = m.group->modelScale.x;
			if (ImGui::DragFloat("scale", &s, 0.005f, 0.01f, 5.0f))
			{
				m.group->modelScale = glm::vec3(s);   // 保持等比，避免法线被拉歪
				changed = true;
			}

			// 位置变了就重算碰撞盒，否则玩家会撞到旧位置的空气墙
			if (changed) gScene.RefreshBox(*m.group);

			ImGui::PopID();
		}

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
