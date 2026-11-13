#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "CShader.h"
#include "CMesh.h"
#include "CCollision.h"

// ============ 相机状态（第一人称） ============
glm::vec3 cameraPos(0.0f, 1.7f, 8.0f);
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

	GLFWwindow* window = glfwCreateWindow(1280, 720, "Horror Demo", nullptr, nullptr);
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

	glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// ================= 场景：一个 20×20×4 的封闭房间 =================
	std::vector<float> verts;
	std::vector<unsigned int> indices;
	std::vector<Box> boxes;          // 碰撞盒：由 appendBoxMesh 顺手收集
	boxes.reserve(16);

	const glm::vec3 floorColor(0.35f, 0.32f, 0.28f);
	const glm::vec3 wallColor(0.45f, 0.43f, 0.40f);
	const glm::vec3 pillarColor(0.30f, 0.22f, 0.18f);
	const glm::vec3 crateColor(0.50f, 0.38f, 0.22f);

	appendBoxMesh(verts, indices, -10, -0.5f, -10, 10, 0, 10, floorColor, boxes);       // 地板
	appendBoxMesh(verts, indices, -10, 4, -10, 10, 4.5f, 10, wallColor, boxes);         // 天花板
	appendBoxMesh(verts, indices, -10.5f, 0, -10.5f, 10.5f, 4, -10, wallColor, boxes);  // 北墙
	appendBoxMesh(verts, indices, -10.5f, 0, 10, 10.5f, 4, 10.5f, wallColor, boxes);    // 南墙
	appendBoxMesh(verts, indices, -10.5f, 0, -10, -10, 4, 10, wallColor, boxes);        // 西墙
	appendBoxMesh(verts, indices, 10, 0, -10, 10.5f, 4, 10, wallColor, boxes);          // 东墙

	for (float x : {-5.0f, 5.0f})                                          // 4 根柱子
		for (float z : {-5.0f, 5.0f})
			appendBoxMesh(verts, indices, x - 0.4f, 0, z - 0.4f, x + 0.4f, 4, z + 0.4f, pillarColor, boxes);

	appendBoxMesh(verts, indices, 2.0f, 0, -3.0f, 3.2f, 1.2f, -1.8f, crateColor, boxes);          // 几个木箱
	appendBoxMesh(verts, indices, -4.0f, 0, 2.5f, -2.8f, 1.2f, 3.7f, crateColor, boxes);
	appendBoxMesh(verts, indices, -3.8f, 1.2f, 2.7f, -3.0f, 2.0f, 3.5f, crateColor, boxes);       // 叠一个
	appendBoxMesh(verts, indices, 6.5f, 0, 6.5f, 7.7f, 1.2f, 7.7f, crateColor, boxes);

	CMesh cMesh;
	cMesh.Upload(verts, indices);

	std::cout << "[main] collision boxes: " << boxes.size() << "\n";

	CShader shader("assets/shaders/horror.vert", "assets/shaders/horror.frag");

	while (!glfwWindowShouldClose(window))
	{
		float curFrame = (float)glfwGetTime();
		deltaTime = curFrame - lastFrame;
		lastFrame = curFrame;

		processInput(window, boxes);

		// 手电筒开关
		if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)     flashlightOn = true;
		if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)     flashlightOn = false;

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glm::mat4 projection = glm::perspective(glm::radians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
		glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

		// 手电筒 = 相机位置 + 相机朝向（加一点手持摇摆）
		float sway = sin(curFrame * 1.8f) * 0.015f;
		glm::vec3 right = glm::normalize(glm::cross(cameraFront, cameraUp));
		glm::vec3 lightDir = glm::normalize(cameraFront + right * sway);

		shader.use();
		shader.setMat4("projection", projection);
		shader.setMat4("view", view);
		shader.setVec3("viewPos", cameraPos);
		shader.setVec3("lightPos", cameraPos);
		shader.setVec3("lightDir", lightDir);
		shader.setVec3("lightColor", flashlightOn ? glm::vec3(2.5f, 2.5f, 2.5f) : glm::vec3(0.0f));
		shader.setFloat("cutoff", glm::cos(glm::radians(10.0f)));      // 内锥 10°
		shader.setFloat("outerCutoff", glm::cos(glm::radians(25.0f))); // 外锥 25°



		cMesh.Draw();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}
