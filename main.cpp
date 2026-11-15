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
#include "CMesh.h"
#include "CCollision.h"
#include "CShadow.h"

struct CLight
{
	glm::vec3 Position;
	glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 color;
	float intensity = 1.0f;
	float range = 10.0f;
	// 内锥角（度），传 shader 前再转 cos
	float cutoffDeg = 30.0f;
	// 外锥角（度），阴影 Pass 直接用
	float outerCutoffDeg = 45.0f;
	// 0为点光源 1为聚光灯
	int type = 1;

};

std::vector<CLight> lights;
std::vector<std::unique_ptr<CShadow>> shadowPasses;

// ============ 相机状态（第一人称） ============
glm::vec3 cameraPos(1.0f, 1.7f, 9.2f);
glm::vec3 cameraFront(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);
float yaw = -90.0f;
float pitch = 0.0f;
float lastX = 640.0f, lastY = 360.0f;
bool  firstMouse = true;
float deltaTime = 0.0f, lastFrame = 0.0f;
// 手电筒开关
bool flashlightOn = true;
// 鼠标是否锁定：true = 转视角（光标隐藏），false = 显示光标（可点 UI）
bool mouseCaptured = true;

// 玩家碰撞半径（肩宽的一半）：越大离墙越远
constexpr float kPlayerRadius = 0.3f;
// 玩家眼睛到脚底的高度
constexpr float kPlayerHeight = 1.7f;

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
	float xoffset = (xpos - lastX) * 0.1f;
	float yoffset = (lastY - ypos) * 0.1f;
	lastX = xpos; lastY = ypos;
	yaw += xoffset;
	pitch += yoffset;
	pitch = glm::clamp(pitch, -89.0f, 89.0f);
	// 固定垂直视角在89度以内，防止view矩阵为0
	cameraFront = glm::normalize(glm::vec3(
		cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
		sin(glm::radians(pitch)),
		sin(glm::radians(yaw)) * cos(glm::radians(pitch))));
}

// 键盘回调：只在「按下」瞬间触发一次，适合做开关类操作（Tab 切换光标）
void keyCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
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

void processInput(GLFWwindow* window, const std::vector<Box>& boxes)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (!mouseCaptured) return;      // Tab 放开鼠标时冻结移动

	// ① 先算出「期望位移」
	float speed = 3.0f * deltaTime;
	glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
	glm::vec3 delta(0.0f);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) delta += cameraFront * speed;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) delta -= cameraFront * speed;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) delta -= right * speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) delta += right * speed;

	// ② 再交给碰撞求解，决定实际能走多少
	if (glm::length(delta) > 1e-6f)
		cameraPos = CCollision::MoveWithCollision(cameraPos, delta, kPlayerRadius, kPlayerHeight, boxes);
}

/**
 * 将AABB转换成顶点数据和索引数据
 */
void appendBoxMesh(std::vector<float>& verts, std::vector<unsigned int>& indices,
	float x0, float y0, float z0, float x1, float y1, float z1,
	const glm::vec3& color, std::vector<Box>& boxes)
{
	unsigned int base = (unsigned int)(verts.size() / 11);   // 每个顶点 11 个 float
	struct Face { glm::vec3 n; glm::vec3 v[4]; };
	Face faces[6] = {
		{ { 0, 0, 1}, {{x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1}} },  // +Z
		{ { 0, 0,-1}, {{x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0}} },  // -Z
		{ { 1, 0, 0}, {{x1,y0,z1},{x1,y0,z0},{x1,y1,z0},{x1,y1,z1}} },  // +X
		{{-1, 0, 0}, {{x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0}} },   // -X
		{ { 0, 1, 0}, {{x0,y1,z1},{x1,y1,z1},{x1,y1,z0},{x0,y1,z0}} },  // +Y
		{ { 0,-1, 0}, {{x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1}} },  // -Y
	};
	// 每个面 4 个角的 UV，顺序要和 Face.v 一致：左下、右下、右上、左上
	static const glm::vec2 kQuadUV[4] = { {0.0f,0.0f}, {1.0f,0.0f}, {1.0f,1.0f}, {0.0f,1.0f} };

	for (auto& f : faces)
	{
		for (int i = 0; i < 4; ++i)
		{
			const glm::vec3& c = f.v[i];
			verts.push_back(c.x); verts.push_back(c.y); verts.push_back(c.z);
			verts.push_back(f.n.x); verts.push_back(f.n.y); verts.push_back(f.n.z);
			verts.push_back(color.r); verts.push_back(color.g); verts.push_back(color.b);
			verts.push_back(kQuadUV[i].x); verts.push_back(kQuadUV[i].y);   // ← 新增 UV
		}
		indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
		base += 4;
	}

	// 顺手记录碰撞盒：渲染和碰撞同源，永远不会脱节
	boxes.push_back({ glm::vec3(x0, y0, z0), glm::vec3(x1, y1, z1) });
}

int main()
{
	std::cout << "[main] working dir: " << std::filesystem::current_path().string() << "\n";

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(2440, 1240, "Horror Demo", nullptr, nullptr);
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

	// ================= 场景：一个 20×20×4 的封闭房间 =================
	std::vector<float> verts;
	std::vector<unsigned int> indices;
	std::vector<Box> boxes;          // 碰撞盒：由 appendBoxMesh 顺手收集
	boxes.reserve(16);
	 
	const glm::vec3 floorColor(0.35f, 0.32f, 0.28f);
	const glm::vec3 wallColor(0.45f, 0.43f, 0.40f);
	const glm::vec3 pillarColor(0.30f, 0.22f, 0.18f);
	const glm::vec3 crateColor(0.50f, 0.38f, 0.22f);

	// ===== 布局 =====
	//   厨房/餐厅 : x -10..4,  z 0..10
	//   走廊      : x -10..-7, z -16..0  (3 米宽, 16 米长)
	//   洗手间    : x -7..-1,  z -15..-11 (6 x 4)，门在走廊右侧 z -14..-12.4（1.6 米宽）

	// 地面 / 天花板（三块，跟着房间走）
	appendBoxMesh(verts, indices, -10.0f, -0.5f, 0.0f, 4.0f, 0.0f, 10.0f, floorColor, boxes);
	appendBoxMesh(verts, indices, -10.0f, -0.5f, -16.0f, -7.0f, 0.0f, 0.0f, floorColor, boxes);
	appendBoxMesh(verts, indices, -7.0f, -0.5f, -15.0f, -1.0f, 0.0f, -11.0f, floorColor, boxes);

	appendBoxMesh(verts, indices, -10.0f, 4.0f, 0.0f, 4.0f, 4.5f, 10.0f, wallColor, boxes);
	appendBoxMesh(verts, indices, -10.0f, 4.0f, -16.0f, -7.0f, 4.5f, 0.0f, wallColor, boxes);
	appendBoxMesh(verts, indices, -7.0f, 4.0f, -15.0f, -1.0f, 4.5f, -11.0f, wallColor, boxes);

	// ---- 厨房 / 餐厅 ----
	appendBoxMesh(verts, indices, -10.5f, 0.0f, 10.0f, 0.0f, 4.0f, 10.5f, wallColor, boxes);  // 南墙（门洞 x 0..2）
	appendBoxMesh(verts, indices, 2.0f, 0.0f, 10.0f, 4.5f, 4.0f, 10.5f, wallColor, boxes);
	appendBoxMesh(verts, indices, 4.0f, 0.0f, -0.5f, 4.5f, 4.0f, 10.5f, wallColor, boxes);  // 东墙
	appendBoxMesh(verts, indices, -10.5f, 0.0f, -0.5f, -10.0f, 4.0f, 10.5f, wallColor, boxes); // 西墙
	appendBoxMesh(verts, indices, -7.0f, 0.0f, -0.5f, 4.5f, 4.0f, 0.0f, wallColor, boxes);  // 北墙（走廊开口 x -10..-7）

	// ---- 走廊（3 米宽，16 米长）----
	appendBoxMesh(verts, indices, -10.5f, 0.0f, -16.5f, -10.0f, 4.0f, 0.0f, wallColor, boxes); // 西墙
	appendBoxMesh(verts, indices, -10.5f, 0.0f, -16.5f, -6.5f, 4.0f, -16.0f, wallColor, boxes); // 尽头墙
	appendBoxMesh(verts, indices, -7.0f, 0.0f, -16.0f, -6.5f, 4.0f, -14.0f, wallColor, boxes); // 东墙（北段）
	appendBoxMesh(verts, indices, -7.0f, 0.0f, -12.4f, -6.5f, 4.0f, 0.0f, wallColor, boxes); // 东墙（南段）

	// ---- 洗手间（6 x 4，门 1.6 米宽）----
	appendBoxMesh(verts, indices, -7.0f, 0.0f, -15.5f, -0.5f, 4.0f, -15.0f, wallColor, boxes); // 北墙
	appendBoxMesh(verts, indices, -1.0f, 0.0f, -15.5f, -0.5f, 4.0f, -10.5f, wallColor, boxes); // 东墙
	appendBoxMesh(verts, indices, -7.0f, 0.0f, -11.0f, -0.5f, 4.0f, -10.5f, wallColor, boxes); // 南墙

	// ---- 厨房家具 ----
	const glm::vec3 woodColor(0.32f, 0.20f, 0.12f);
	const glm::vec3 candleColor(0.92f, 0.86f, 0.70f);

	appendBoxMesh(verts, indices, -4.6f, 0.0f, 3.9f, -1.4f, 0.78f, 7.1f, woodColor, boxes);       // 餐桌
	appendBoxMesh(verts, indices, -3.06f, 0.78f, 5.44f, -2.90f, 1.14f, 5.60f, candleColor, boxes); // 蜡烛
	appendBoxMesh(verts, indices, -5.6f, 0.0f, 4.4f, -4.9f, 0.9f, 5.1f, woodColor, boxes);         // 椅子
	appendBoxMesh(verts, indices, -1.1f, 0.0f, 4.4f, -0.4f, 0.9f, 5.1f, woodColor, boxes);
	appendBoxMesh(verts, indices, -5.6f, 0.0f, 5.9f, -4.9f, 0.9f, 6.6f, woodColor, boxes);
	appendBoxMesh(verts, indices, -1.1f, 0.0f, 5.9f, -0.4f, 0.9f, 6.6f, woodColor, boxes);
	appendBoxMesh(verts, indices, 2.2f, 0.0f, 0.6f, 3.9f, 2.4f, 2.0f, pillarColor, boxes);       // 壁炉

	// ---- 洗手间 ----
	appendBoxMesh(verts, indices, -6.0f, 0.0f, -14.6f, -4.0f, 0.9f, -13.9f, wallColor, boxes);     // 洗手台
	appendBoxMesh(verts, indices, -6.1f, 1.2f, -15.0f, -3.9f, 2.7f, -14.9f, pillarColor, boxes);   // 镜子

	CMesh cMesh;
	cMesh.Upload(verts, indices);

	std::cout << "[main] collision boxes: " << boxes.size() << "\n";

	CShader shader("assets/shaders/horror.vert", "assets/shaders/horror.frag");

	// ================= 光源列表（复用全局 lights） =================
	lights.reserve(8);

	// 0 号灯：壁炉（固定位置，唯一投影的灯）
	CLight fireplace;
	fireplace.type            = 1;
	fireplace.color           = glm::vec3(1.0f, 0.55f, 0.25f);
	fireplace.intensity       = 10.0f;
	fireplace.Position        = glm::vec3(3.0f, 1.2f, 2.4f);   // 火口外侧，别埋进壁炉盒子里
	fireplace.direction       = glm::normalize(glm::vec3(-1.0f, -0.30f, 1.0f));
	fireplace.cutoffDeg       = 14.0f;
	fireplace.outerCutoffDeg  = 30.0f;
	lights.push_back(fireplace);

	CLight flashlight;
	flashlight.type           = 1;
	flashlight.color          = glm::vec3(1.0f);
	flashlight.intensity      = 2.5f;
	flashlight.cutoffDeg      = 10.0f;
	flashlight.outerCutoffDeg = 25.0f;
	lights.push_back(flashlight);

	CLight lamp = fireplace;
	lamp.Position = glm::vec3(-4.0f, 3.0f, -13.0f);
	lamp.direction = glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f));
	lights.push_back(lamp);

	// 3 号：蜡烛（点光源，不投影）
	CLight candle;
	candle.type      = 0;
	candle.color     = glm::vec3(1.0f, 0.62f, 0.30f);
	candle.intensity = 2.2f;
	candle.Position  = glm::vec3(-2.98f, 1.15f, 5.52f);
	lights.push_back(candle);

	// 每帧往 shader 传的数组缓冲（复用，避免每帧重新分配）
	std::vector<glm::vec3> lPos, lDir, lColor;
	std::vector<float>     lIntensity, lCutoff, lOuterCutoff;
	std::vector<int>       lType;
	std::vector<glm::mat4> shadowMatrices;
	for (const CLight& l : lights)
	{
		shadowPasses.push_back(std::make_unique<CShadow>(1024));
	}
		
	while (!glfwWindowShouldClose(window))
	{
		float curFrame = (float)glfwGetTime();
		deltaTime = curFrame - lastFrame;
		lastFrame = curFrame;

		processInput(window, boxes);

		const int lightCount = (int)lights.size();

		// 手电筒开关
		if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)     flashlightOn = true;
		if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)     flashlightOn = false;

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		int fbw = 0, fbh = 0;
		glfwGetFramebufferSize(window, &fbw, &fbh);
		glm::mat4 projection = glm::perspective(glm::radians(60.0f),
			(float)fbw / (float)fbh, 0.1f, 100.0f);
		glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

		// 手电筒 = 相机位置 + 相机朝向（加一点手持摇摆）
		float sway = sin(curFrame * 1.8f) * 0.015f;
		glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
		glm::vec3 lightDir = glm::normalize(cameraFront + right * sway);

		// 1 号灯：手电筒跟随相机（不投影）
		lights[1].Position  = cameraPos;
		lights[1].direction = lightDir;
		lights[1].intensity = flashlightOn ? 2.5f : 0.0f;

		// 第一遍：从光源视角把场景渲染进各自的深度图
		shadowMatrices.assign(lightCount, glm::mat4(1.0f));

		for (int i = 0; i < lightCount; ++i)
		{
			if (lights[i].type != 1) continue;   // 点光源不能用 2D 深度图，先跳过
			shadowPasses[i]->Render(cMesh, lights[i].Position, lights[i].direction,
				lights[i].outerCutoffDeg, 0.1f, 50.0f);
			shadowMatrices[i] = shadowPasses[i]->GetLightSpace();
		}

		// 打包成 shader 需要的数组
		lPos.clear(); lDir.clear(); lColor.clear();
		lIntensity.clear(); lCutoff.clear(); lOuterCutoff.clear(); lType.clear();
		for (const CLight& l : lights)
		{
			lPos.push_back(l.Position);
			lDir.push_back(l.direction);
			lColor.push_back(l.color);
			lIntensity.push_back(l.intensity);
			lCutoff.push_back(glm::cos(glm::radians(l.cutoffDeg)));
			lOuterCutoff.push_back(glm::cos(glm::radians(l.outerCutoffDeg)));
			lType.push_back(l.type);
		}

		shader.use();
		shader.setMat4("projection", projection);
		shader.setMat4("view", view);
		shader.setVec3("viewPos", cameraPos);


		shader.setInt("lightCount", lightCount);
		shader.setVec3Array("lightPos[0]",          lPos.data(),           lightCount);
		shader.setVec3Array("lightDirArr[0]",       lDir.data(),           lightCount);
		shader.setVec3Array("lightColorArr[0]",     lColor.data(),         lightCount);
		shader.setFloatArray("lightIntensity[0]",   lIntensity.data(),     lightCount);
		shader.setFloatArray("lightCutoff[0]",      lCutoff.data(),        lightCount);
		shader.setFloatArray("lightOuterCutoff[0]", lOuterCutoff.data(),   lightCount);
		shader.setIntArray("lightType[0]",          lType.data(),          lightCount);

		// 纹理单元计数器
		int shadowUnit = 0;

		for (int i = 0; i < lightCount; ++i)
		{
			// 跳过点光源 因为点光源不用2D阴影贴图
			if (lights[i].type != 1)
			{
				continue;
			}
			glActiveTexture(GL_TEXTURE0 + shadowUnit);
			glBindTexture(GL_TEXTURE_2D, shadowPasses[i]->GetDepthTexture());
			shader.setInt("shadowMap" + std::to_string(i), shadowUnit);
			++shadowUnit;
		}
		// 光源的相机矩阵
		shader.setMat4Array("lightSpaceMatrices[0]", shadowMatrices.data(), lightCount);

		cMesh.Draw();

		// ImGui
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(460, 640), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
		ImGui::Begin("ImGui");
		ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);

		for (int i = 0; i < (int)lights.size(); ++i)
		{
			ImGui::PushID(i);
			ImGui::Text("Light %d", i);
			ImGui::DragFloat3("position", &lights[i].Position.x, 0.05f);
			ImGui::SliderFloat("intensity", &lights[i].intensity, 0.0f, 20.0f);
			ImGui::ColorEdit3("color", &lights[i].color.x);
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
