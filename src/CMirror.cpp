#include "CMirror.h"

#include <glad/glad.h>

#include <algorithm>
#include <iostream>

#include "CScene.h"
#include "CDeferred.h"

namespace
{
	// Lengyel 的斜近裁剪面：把投影矩阵的近平面换成任意平面（相机空间），
	// 用来把镜面背后的几何裁掉。plane 的 w 分量要满足 dot(n, p) - d = 0。
	glm::mat4 ObliqueProjection(const glm::mat4& proj, const glm::vec4& plane)
	{
		glm::mat4 m = proj;

		// glm 是列主序：m[col][row]，所以「矩阵第 2 行」的系数是 m[0][2],m[1][2],m[2][2],m[3][2]
		glm::vec4 q;
		q.x = (glm::sign(plane.x) + m[0][2]) / m[0][0];
		q.y = (glm::sign(plane.y) + m[1][2]) / m[1][1];
		q.z = -1.0f;
		q.w = (1.0f + m[2][2]) / m[3][2];

		const float denom = glm::dot(plane, q);
		if (std::fabs(denom) < 1e-5f)
			return proj;

		const glm::vec4 c = plane * (2.0f / denom);

		m[0][2] = c.x;
		m[1][2] = c.y;
		m[2][2] = c.z + 1.0f;
		m[3][2] = c.w;
		return m;
	}
}

CMirror::CMirror(int width, int height)
	: m_shader("assets/shaders/mirror.vert", "assets/shaders/mirror.frag"),
	  m_gbuf(std::max(width / 2, 1), std::max(height / 2, 1))
{
	m_width  = std::max(width  / 2, 1);
	m_height = std::max(height / 2, 1);
	m_screenW = std::max(width, 1);
	m_screenH = std::max(height, 1);

	// 镜面方片：位置 + uv，一个 quad 用 strip 画
	const float verts[] = {
		-1.0f, -1.0f, 0.0f,   0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,   1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,   0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f,   1.0f, 1.0f,
	};

	glGenVertexArrays(1, &m_quadVao);
	glGenBuffers(1, &m_quadVbo);

	glBindVertexArray(m_quadVao);
	glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

	glBindVertexArray(0);

	CreateFBO();
}

CMirror::~CMirror()
{
	DestroyFBO();

	if (m_quadVbo) glDeleteBuffers(1, &m_quadVbo);
	if (m_quadVao) glDeleteVertexArrays(1, &m_quadVao);
}

void CMirror::CreateFBO()
{
	glGenFramebuffers(1, &m_fbo);
	glGenTextures(1, &m_colorTex);
	glGenRenderbuffers(1, &m_depthRbo);

	// 反射画面可能是 > 1.0 的线性值，用 RGBA16F
	glBindTexture(GL_TEXTURE_2D, m_colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "[CMirror] FBO incomplete!\n";

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CMirror::DestroyFBO()
{
	if (m_colorTex) glDeleteTextures(1, &m_colorTex);
	if (m_depthRbo) glDeleteRenderbuffers(1, &m_depthRbo);
	if (m_fbo)      glDeleteFramebuffers(1, &m_fbo);

	m_colorTex = m_depthRbo = m_fbo = 0;
}

void CMirror::Resize(int width, int height)
{
	if (width <= 0 || height <= 0) return;

	const int w = std::max(width  / 2, 1);
	const int h = std::max(height / 2, 1);
	if (w == m_width && h == m_height)
	{
		m_screenW = width;
		m_screenH = height;
		return;
	}

	m_width  = w;
	m_height = h;
	m_screenW = width;
	m_screenH = height;

	glBindTexture(GL_TEXTURE_2D, m_colorTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
		GL_RGBA, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	m_gbuf.Resize(m_width, m_height);
}

void CMirror::SetSurface(const glm::vec3& center, const glm::vec3& normal,
	float halfWidth, float halfHeight)
{
	m_center = center;
	m_normal = glm::normalize(normal);
	m_halfW  = halfWidth;
	m_halfH  = halfHeight;
}

void CMirror::RenderReflection(CScene& scene, CShader& gbufferShader, CDeferred& deferred,
	const glm::mat4& projection, const glm::mat4& mainView,
	const glm::vec3& cameraPos)
{
	if (!IsValid()) return;

	const glm::vec3 n = glm::normalize(m_normal);
	const float     d = glm::dot(m_center, n);

	// 关于平面 dot(n,p)=d 的镜像矩阵：p' = p - 2*(dot(n,p) - d)*n
	glm::mat4 R(1.0f);
	R[0][0] = 1.0f - 2.0f * n.x * n.x;
	R[1][0] =       -2.0f * n.x * n.y;
	R[2][0] =       -2.0f * n.x * n.z;
	R[3][0] =        2.0f * d * n.x;

	R[0][1] =       -2.0f * n.y * n.x;
	R[1][1] = 1.0f - 2.0f * n.y * n.y;
	R[2][1] =       -2.0f * n.y * n.z;
	R[3][1] =        2.0f * d * n.y;

	R[0][2] =       -2.0f * n.z * n.x;
	R[1][2] =       -2.0f * n.z * n.y;
	R[2][2] = 1.0f - 2.0f * n.z * n.z;
	R[3][2] =        2.0f * d * n.z;

	// 镜像后的相机：位置也跟着翻，view 直接左乘反射矩阵
	const glm::vec3 mirrorCamPos = glm::vec3(R * glm::vec4(cameraPos, 1.0f));
	const glm::mat4 mirrorView   = mainView * R;

	// 斜近裁剪面：世界平面 dot(n,p) - d = 0 转到镜像相机的 view 空间
	// 平面变换要乘 (view^-1)^T，不能直接用 transpose(view)（view 不是正交矩阵，
	// 平移那列会算错，结果是镜子背后的几何裁不掉，镜子里能看到墙后的房间）
	const glm::mat4 invMirrorView = glm::inverse(mirrorView);
	glm::vec4 planeV = glm::transpose(invMirrorView) * glm::vec4(n, -d);
	const glm::vec3 centerV = glm::vec3(mirrorView * glm::vec4(m_center, 1.0f));
	if (glm::dot(glm::vec3(planeV), centerV) < 0.0f)
		planeV = -planeV;

	const glm::mat4 mirrorProj = ObliqueProjection(projection, planeV);
	m_mirrorVP = mirrorProj * mirrorView;

	// ---- 反射画面的 G-buffer Pass（半分辨率）----
	// 几何画进 m_gbuf，深度也在这里，后面光照 Pass 逐像素读它
	m_gbuf.Begin();

	// 镜像把三角形绕序翻过来了，不切回来会看到背面（墙面会"消失"）
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CW);
	scene.RenderGeometryTo(gbufferShader, mirrorView, mirrorProj);
	glFrontFace(GL_CCW);

	m_gbuf.End();

	// ---- 反射画面的光照 Pass：和主画面同一条 PBR + IBL 路径 ----
	GLint prevFbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
	GLint prevVp[4] = { 0, 0, 0, 0 };
	glGetIntegerv(GL_VIEWPORT, prevVp);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glViewport(0, 0, m_width, m_height);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// 传镜像相机位置：反射画面的高光/菲涅尔要按这个视点算
	scene.SetupDeferredLightingFor(deferred.Shader(), mirrorView, mirrorProj, mirrorCamPos);
	deferred.Render(m_gbuf.GetAlbedoTexture(), m_gbuf.GetNormalTexture(),
		m_gbuf.GetPositionTexture());

	glEnable(GL_DEPTH_TEST);   // deferred.Render 会关掉它，后面的主 Pass 还要用

	glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
	glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
}

void CMirror::Draw(CScene& scene, const glm::mat4& projection, const glm::mat4& mainView,
	const glm::vec3& cameraPos)
{
	if (!enabled || !IsValid()) return;

	// 镜面方片的世界变换：中心 + 沿法线朝房间挪 6mm（压住模型自带的玻璃面）
	const glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), m_normal));
	const glm::vec3 up    = glm::cross(m_normal, right);
	const glm::vec3 center = m_center + m_normal * 0.006f;

	glm::mat4 model(1.0f);
	model[0] = glm::vec4(right * m_halfW, 0.0f);
	model[1] = glm::vec4(up    * m_halfH, 0.0f);
	model[2] = glm::vec4(m_normal, 0.0f);
	model[3] = glm::vec4(center, 1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	// 光源数组：镜面反光要按真实光照算（内部会 shader.use()）
	scene.UploadLightingUniforms(m_shader);

	m_shader.setMat4("uProjection", projection);
	m_shader.setMat4("uView", mainView);
	m_shader.setMat4("model", model);
	m_shader.setMat4("uMirrorVP", m_mirrorVP);
	m_shader.setFloat("uTint", tint);
	m_shader.setFloat("uEdgeFade", edgeFade);
	m_shader.setVec2("uInvScreen", glm::vec2(1.0f / (float)std::max(m_screenW, 1),
		                                     1.0f / (float)std::max(m_screenH, 1)));
	m_shader.setInt("uReflection", 0);

	// 镜面高光所需的法线与视点
	m_shader.setVec3("uNormal", m_normal);
	m_shader.setVec3("viewPos", cameraPos);
	m_shader.setFloat("uSpecStrength", specStrength);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_colorTex);

	glBindVertexArray(m_quadVao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glEnable(GL_CULL_FACE);
}
