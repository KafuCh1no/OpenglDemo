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

	CTexture wallTex("assets/textures/Plaster006_2K-JPG_Color.jpg");
	CTexture wallNormal("assets/textures/Plaster006_2K-JPG_NormalGL.jpg", false);
	CTexture paintedWallTex("assets/textures/PaintedPlaster014_2K-JPG_Color.jpg");
	CTexture paintedWallNormal("assets/textures/PaintedPlaster014_2K-JPG_NormalGL.jpg", false);

	// 四种材质分组（现在都没贴图，靠顶点色区分；以后放上贴图即可）
	// uScale 现在是「每米重复几次」：0.4 = 每 2.5 米一个循环
	auto& gWall        = gScene.NewGroup({ &wallTex,        glm::vec3(1.0f), 0.4f, &wallNormal,        1.0f });
	auto& gPaintedWall = gScene.NewGroup({ &paintedWallTex, glm::vec3(1.0f), 0.25f, &paintedWallNormal, 1.0f });
	auto& gFloor = gScene.NewGroup({ nullptr, glm::vec3(1.0f), 2.0f });
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

	// ---- 厨房家具（木） ----
	gScene.AddBox(gWood, -3.06f, 0.78f, 5.44f, -2.90f, 1.14f, 5.60f, candleColor); // 蜡烛
	gScene.AddBox(gWood, -5.6f, 0.0f, 4.4f, -4.9f, 0.9f, 5.1f, woodColor);     // 椅子
	gScene.AddBox(gWood, -1.1f, 0.0f, 4.4f, -0.4f, 0.9f, 5.1f, woodColor);
	gScene.AddBox(gWood, -5.6f, 0.0f, 5.9f, -4.9f, 0.9f, 6.6f, woodColor);
	gScene.AddBox(gWood, -1.1f, 0.0f, 5.9f, -0.4f, 0.9f, 6.6f, woodColor);

	// ---- 石砌 / 镜面 ----
	gScene.AddBox(gStone, 2.2f, 0.0f, 0.6f, 3.9f, 2.4f, 2.0f, pillarColor);        // 壁炉
	gScene.AddBox(gStone, -6.0f, 0.0f, -14.6f, -4.0f, 0.9f, -13.9f, wallColor);    // 洗手台
	gScene.AddBox(gStone, -6.1f, 1.2f, -15.0f, -3.9f, 2.7f, -14.9f, pillarColor);  // 镜子

	// 模型
	CTexture tableAlbedo("assets/models/table-damaged/textures/Table-Damage_low_albedo.jpeg");
	CTexture tableNormal("assets/models/table-damaged/textures/Table-Damage_low_normal.jpeg", false);

	// 原模型约 52 x 22 x 28 单位，缩到约 1.3 x 0.57 x 0.7 米
	// 位置放在相机正前方（相机起始于 (1, 1.7, 9.2) 朝 -Z）
	gScene.AddModel("assets/models/table-damaged/source/Table-Damage_low.obj",
		glm::vec3(-2.5f, 0.0f, 6.0f),      // 位置
		glm::vec3(0.0f),                   // 旋转（度）
		glm::vec3(0.055f),                 // 缩放
		{ &tableAlbedo, glm::vec3(1.0f), 1.0f, &tableNormal, 1.0f });

	gScene.Build();
	gScene.PrepareShadows(3);   // 3 盏聚光灯投影：壁炉 / 手电筒 / 洗手间
	std::cout << "[main] collision boxes: " << gScene.Boxes().size() << "\n";

	// ================= 灯光 =================
	// 0 号：壁炉（固定位置，投影）
	CLight fireplace;
	fireplace.type           = 1;
	fireplace.color          = glm::vec3(1.0f, 0.55f, 0.25f);
	fireplace.intensity      = 10.0f;
	fireplace.Position       = glm::vec3(3.0f, 1.2f, 2.4f);
	fireplace.direction      = glm::normalize(glm::vec3(-1.0f, -0.30f, 1.0f));
	fireplace.cutoffDeg      = 14.0f;
	fireplace.outerCutoffDeg = 30.0f;
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
	bathroom.Position  = glm::vec3(-4.0f, 3.0f, -13.0f);
	bathroom.direction = glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f));
	gScene.AddLight(bathroom);

	// 3 号：蜡烛（点光源，不投影）
	CLight candle;
	candle.type      = 0;
	candle.color     = glm::vec3(1.0f, 0.62f, 0.30f);
	candle.intensity = 2.2f;
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
