#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "CShader.h"
#include "CScene.h"
#include "CTexture.h"

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
constexpr float kPlayerHeight = 1.7f;

// 手电筒在光源列表里的下标
constexpr int kFlashlightIndex = 1;

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
	const float speed = 3.0f * deltaTime;
	const glm::vec3 front = gScene.GetFront();
	const glm::vec3 right = gScene.GetRight();
	glm::vec3 delta(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += front * speed;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= front * speed;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= right * speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += right * speed;

	// ② 再交给场景的碰撞求解，决定实际能走多少
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
	ImGui::GetIO().FontGlobalScale = 1.7f;

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// 场景：按材质分组
	const glm::vec3 floorColor(0.35f, 0.32f, 0.28f);
	const glm::vec3 wallColor(0.95f, 0.93f, 0.9f);
	const glm::vec3 paintedWallColor(0.95f, 0.93f, 0.9f);
	const glm::vec3 pillarColor(0.30f, 0.22f, 0.18f);
	const glm::vec3 woodColor(0.32f, 0.20f, 0.12f);
	const glm::vec3 candleColor(0.92f, 0.86f, 0.70f);

	// 墙壁
	CTexture wallTex("assets/textures/Plaster006_2K-JPG_Color.jpg");
	CTexture wallNormal("assets/textures/Plaster006_2K-JPG_NormalGL.jpg", false);
	CTexture paintedWallTex("assets/textures/PaintedPlaster014_2K-JPG_Color.jpg");
	CTexture paintedWallNormal("assets/textures/PaintedPlaster014_2K-JPG_NormalGL.jpg", false);

	// 地板
	CTexture floorTex("assets/textures/WoodFloor007_2K-JPG_Color.jpg");
	CTexture floorNormal("assets/textures/WoodFloor007_2K-JPG_NormalGL.jpg", false);

	// 四种材质分组
	auto& gWall        = gScene.NewGroup({ &wallTex,        glm::vec3(1.0f), 0.4f, &wallNormal,        1.0f });
	auto& gPaintedWall = gScene.NewGroup({ &paintedWallTex, glm::vec3(1.0f), 0.25f, &paintedWallNormal, 1.0f });
	auto& gFloor = gScene.NewGroup({ &floorTex, glm::vec3(1.0f), 0.4f, &floorNormal, 1.0f });
	auto& gWood  = gScene.NewGroup({ nullptr, glm::vec3(1.0f), 1.0f });
	auto& gStone = gScene.NewGroup({ nullptr, glm::vec3(1.0f), 1.0f });

	// ---- 地面 / 天花板 ----
	gScene.AddBox(gFloor, -10.0f, -0.5f, 0.0f, 4.0f, 0.0f, 10.0f, floorColor);
	gScene.AddBox(gFloor, -10.0f, -0.5f, -16.0f, -7.0f, 0.0f, 0.0f, floorColor);
	gScene.AddBox(gFloor, -7.0f, -0.5f, -15.0f, -1.0f, 0.0f, -11.0f, floorColor);

	gScene.AddBox(gFloor, -10.0f, 4.0f, 0.0f, 4.0f, 4.5f, 10.0f, wallColor);
	gScene.AddBox(gFloor, -10.0f, 4.0f, -16.0f, -7.0f, 4.5f, 0.0f, wallColor);
	gScene.AddBox(gFloor, -7.0f, 4.0f, -15.0f, -1.0f, 4.5f, -11.0f, wallColor);

	// ---- 厨房 / 餐厅墙 ----
	gScene.AddBox(gWall, -10.5f, 0.0f, 10.0f, 0.0f, 4.0f, 10.5f, wallColor);   // 南墙（门洞 x 0..2）
	gScene.AddBox(gWall, 2.0f, 0.0f, 10.0f, 4.5f, 4.0f, 10.5f, wallColor);
	gScene.AddBox(gPaintedWall, 4.0f, 0.0f, -0.5f, 4.5f, 4.0f, 10.5f, paintedWallColor);     // 东墙
	gScene.AddBox(gWall, -10.5f, 0.0f, -0.5f, -10.0f, 4.0f, 10.5f, wallColor); // 西墙
	gScene.AddBox(gWall, -7.0f, 0.0f, -0.5f, 4.5f, 4.0f, 0.0f, wallColor);     // 北墙（走廊开口）

	// ---- 走廊 ----
	gScene.AddBox(gWall, -10.5f, 0.0f, -16.5f, -10.0f, 4.0f, 0.0f, wallColor); // 西墙
	gScene.AddBox(gWall, -10.5f, 0.0f, -16.5f, -6.5f, 4.0f, -16.0f, wallColor); // 尽头墙
	gScene.AddBox(gWall, -7.0f, 0.0f, -16.0f, -6.5f, 4.0f, -14.0f, wallColor);  // 东墙（北段）
	gScene.AddBox(gWall, -7.0f, 0.0f, -12.4f, -6.5f, 4.0f, 0.0f, wallColor);    // 东墙（南段）

	// ---- 洗手间 ----
	gScene.AddBox(gWall, -7.0f, 0.0f, -15.5f, -0.5f, 4.0f, -15.0f, wallColor); // 北墙
	gScene.AddBox(gWall, -1.0f, 0.0f, -15.5f, -0.5f, 4.0f, -10.5f, wallColor); // 东墙
	gScene.AddBox(gWall, -7.0f, 0.0f, -11.0f, -0.5f, 4.0f, -10.5f, wallColor); // 南墙

	// ---- 石砌 / 镜面 ----
	// 壁炉改用模型（见下方 AddModel），原来的 5 块石盒子已移除
	gScene.AddBox(gStone, -6.0f, 0.0f, -14.6f, -4.0f, 0.9f, -13.9f, wallColor);    // 洗手台
	gScene.AddBox(gStone, -6.1f, 1.2f, -15.0f, -3.9f, 2.7f, -14.9f, pillarColor);  // 镜子

	// 模型
	// 桌子
	CTexture tableAlbedo("assets/models/table-damaged/textures/Table-Damage_low_albedo.jpeg");
	CTexture tableNormal("assets/models/table-damaged/textures/Table-Damage_low_normal.jpeg", false);

	//椅子
	CTexture chairAlbedo("assets/models/chair/Textures/Blender_2K/T_Chair_BaseColor.jpg");
	CTexture chairNormal("assets/models/chair/Textures/Blender_2K/T_Chair_Normal.png", false);

	// 哥特柜子 / 木桶 / 铜灯笼（Poly Haven, CC0）
	CTexture cabinetAlbedo("assets/models/gothic-cabinet/textures/GothicCabinet_01_diff_2k.jpg");
	CTexture cabinetNormal("assets/models/gothic-cabinet/textures/GothicCabinet_01_nor_gl_2k.jpg", false);

	CTexture barrelAlbedo("assets/models/barrel/textures/Barrel_01_diff.jpg");
	CTexture barrelNormal("assets/models/barrel/textures/Barrel_01_nor.jpg", false);

	CTexture lanternAlbedo("assets/models/lantern/textures/Lantern_01_brass_diff_2k.jpg");
	CTexture lanternNormal("assets/models/lantern/textures/Lantern_01_brass_nor_gl_2k.jpg", false);

	// 吊灯（Poly Haven, CC0）
	CTexture chandelierAlbedo("assets/models/chandelier/textures/Chandelier_02_diff_2k.jpg");
	CTexture chandelierNormal("assets/models/chandelier/textures/Chandelier_02_nor_gl_2k.jpg", false);

	// 壁炉（Sketchfab, CC BY 4.0, by MaX3Dd）
	CTexture fireplaceAlbedo("assets/models/fireplace/textures/fireplace_baseColor.jpg");
	CTexture fireplaceNormal("assets/models/fireplace/textures/fireplace_normal.jpg", false);



	// ---- 可编辑模型表：ImGui 里能拖位置，Reset 回到这里的初值 ----
	struct ModelHandle
	{
		std::string    name;
		CScene::Group* group = nullptr;
		glm::vec3      initPos{ 0.0f };
		glm::vec3      initRot{ 0.0f };
		glm::vec3      initScale{ 1.0f };
	};
	std::vector<ModelHandle> models;
	auto reg = [&models](std::string name, CScene::Group& g)
	{
		models.push_back({ std::move(name), &g, g.modelPos, g.modelRotDeg, g.modelScale });
	};

	// 桌子原模型约 52 x 22 x 28 单位，缩到约 1.3 x 0.57 x 0.7 米
	// 位置放在相机正前方（相机起始于 (1, 1.7, 9.2) 朝 -Z）
	gScene.AddModel("assets/models/table-damaged/source/Table-Damage_low.obj",
		glm::vec3(-1.7f, -0.25f, 6.5f),      // 位置
		glm::vec3(0.0f),                   // 旋转（度）
		glm::vec3(0.065f),                 // 缩放
		{ &tableAlbedo, glm::vec3(1.0f), 1.0f, &tableNormal, 1.0f });

	// 椅子
	gScene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-1.3f, 0.05f, 8.0f),        // ← 挪到桌子左边
		glm::vec3(3.0f, 90.0f, 90.0f),        // ← 转 30°，朝向桌子
		glm::vec3(1.25f),                // 缩放
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	gScene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-2.7f, 0.0f, 8.0f),
		glm::vec3(3.0f, 150.0f, 0.0f),
		glm::vec3(1.25f),
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	gScene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-3.0f, 0.0f, 5.0f),
		glm::vec3(-85.0f, 0.0f, 0.0f),
		glm::vec3(1.25f),
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	gScene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-1.3f, 0.0f, 4.5f),
		glm::vec3(0.0f, 60.0f, 0.0f),
		glm::vec3(1.25f),
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	// 哥特柜子：原始 1.72 x 2.81 x 1.02，底部在 y=-0.45，靠西墙摆放
	{
		auto& g = gScene.AddModel("assets/models/gothic-cabinet/GothicCabinet_01.obj",
			glm::vec3(-9.55f, 0.38f, 3.5f),      // 0.38 = 0.45 * 0.85，让底部贴地
			glm::vec3(0.0f, 90.0f, 0.0f),        // 转 90°，正面朝房间内
			glm::vec3(0.85f),
			{ &cabinetAlbedo, glm::vec3(1.0f), 1.0f, &cabinetNormal, 1.0f });
		reg("Cabinet", g);
	}

	// 木桶：原始 0.56 x 0.88 x 0.56，放在客厅西南角
	{
		auto& g = gScene.AddModel("assets/models/barrel/Barrel_01.obj",
			glm::vec3(-9.0f, 0.01f, 1.2f),
			glm::vec3(0.0f),
			glm::vec3(1.0f),
			{ &barrelAlbedo, glm::vec3(1.0f), 1.0f, &barrelNormal, 1.0f });
		reg("Barrel", g);
	}

	// 铜灯笼：原始只有 0.12 x 0.29 x 0.10，放大 2.5 倍放在壁炉前的空地上
	{
		auto& g = gScene.AddModel("assets/models/lantern/Lantern_01.obj",
			glm::vec3(1.2f, 0.0f, 2.9f),
			glm::vec3(0.0f, 20.0f, 0.0f),
			glm::vec3(2.5f),
			{ &lanternAlbedo, glm::vec3(1.0f), 1.0f, &lanternNormal, 1.0f });
		reg("Lantern", g);
	}

	// 吊灯：原始 0.68 x 0.85 x 0.62，最高点在 y=0.908
	{
		auto& g = gScene.AddModel("assets/models/chandelier/Chandelier_02.obj",
			glm::vec3(-2.5f, 2.2f, 3.3f),
			glm::vec3(0.0f),
			glm::vec3(3.0f),
			{ &chandelierAlbedo, glm::vec3(1.0f), 1.0f, &chandelierNormal, 1.0f });
		reg("Chandelier", g);
	}

	// 壁炉：0.83 x 1.69 x 2.18（深 x 高 x 宽），背面贴东墙 x=4.0
	{
		auto& g = gScene.AddModel("assets/models/fireplace/fireplace.obj",
			glm::vec3(2.5f, 0.0f, 0.57f),
			glm::vec3(0.0f, 90.0f, 0.0f),
			glm::vec3(1.5f),
			{ &fireplaceAlbedo, glm::vec3(1.0f), 1.0f, &fireplaceNormal, 1.0f });
		reg("Fireplace", g);
	}

	gScene.Build();
	gScene.PrepareShadows(4);   // 4 盏灯各一套 Pass：壁炉(点) / 手电筒(聚) / 洗手间(聚) / 蜡烛(点)
	std::cout << "[main] collision boxes: " << gScene.Boxes().size() << "\n";

	// ================= 灯光 =================
	// 0 号：壁炉
	CLight fireplace;
	fireplace.type           = 0;      // 点光源：用 cubemap 阴影
	fireplace.color          = glm::vec3(1.0f, 0.55f, 0.25f);
	fireplace.intensity      = 0.3f;
	fireplace.Position       = glm::vec3(2.50f, 0.70f, 0.90f);
	fireplace.castShadow     = true;
	gScene.AddLight(fireplace);

	// 1 号：手电筒（跟随相机，投影）
	CLight flashlight;
	flashlight.type           = 1;
	flashlight.color          = glm::vec3(1.0f);
	flashlight.intensity      = 2.5f;
	flashlight.cutoffDeg      = 10.0f;
	flashlight.outerCutoffDeg = 25.0f;
	flashlight.castShadow     = true;
	flashlight.followCamera   = true;
	gScene.AddLight(flashlight);

	// 2 号：洗手间灯（固定，投影）
	CLight bathroom = fireplace;
	bathroom.type            = 1;                      // 显式改回聚光灯（壁炉已改成点光）
	bathroom.intensity       = 10.0f;
	bathroom.Position        = glm::vec3(-4.0f, 3.0f, -13.0f);
	bathroom.direction       = glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f));
	bathroom.cutoffDeg       = 14.0f;
	bathroom.outerCutoffDeg  = 30.0f;
	gScene.AddLight(bathroom);

	// 3 号：蜡烛（点光源，不投影）
	CLight candle;
	candle.type      = 0;
	candle.color     = glm::vec3(1.0f, 0.62f, 0.30f);
	candle.intensity = 0.0f;
	candle.Position  = glm::vec3(-2.98f, 1.15f, 5.52f);
	candle.castShadow = false;
	gScene.AddLight(candle);

	CShader shader("assets/shaders/horror.vert", "assets/shaders/horror.frag");

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

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int fbw = 0, fbh = 0;
		glfwGetFramebufferSize(window, &fbw, &fbh);
		glm::mat4 projection = glm::perspective(glm::radians(60.0f),
			(float)fbw / (float)fbh, 0.1f, 100.0f);

		// 阴影 Pass + 灯光 uniform + 按材质绘制，全在场景内部完成
		gScene.Render(shader, projection);

		// ================= ImGui =================
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(460, 720), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
		ImGui::Begin("ImGui");
		ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
		ImGui::Separator();

		auto& lights = gScene.Lights();
		for (int i = 0; i < (int)lights.size(); ++i)
		{
			ImGui::PushID(i);
			CLight& l = lights[i];
			ImGui::Text("Light %d (%s)", i, l.type == 1 ? "spot" : "point");
			ImGui::Checkbox("enabled", &l.enabled);
			if (!l.followCamera)
			{
				ImGui::DragFloat3("position", &l.Position.x, 0.05f);
				ImGui::DragFloat3("direction", &l.direction.x, 0.02f);
			}
			ImGui::SliderFloat("intensity", &l.intensity, 0.0f, 20.0f);
			ImGui::SliderFloat("cutoff", &l.cutoffDeg, 1.0f, 80.0f);
			ImGui::SliderFloat("outerCutoff", &l.outerCutoffDeg, 1.0f, 89.0f);
			ImGui::Checkbox("castShadow", &l.castShadow);
			ImGui::ColorEdit3("color", &l.color.x);
			ImGui::Separator();
			ImGui::PopID();
		}

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
