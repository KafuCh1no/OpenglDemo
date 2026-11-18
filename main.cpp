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
