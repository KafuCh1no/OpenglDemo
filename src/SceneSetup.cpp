#include "SceneSetup.h"

#include "CTexture.h"

#include <iostream>
#include <memory>
#include <utility>

namespace
{
	// 贴图集中管理
	std::vector<std::unique_ptr<CTexture>> g_texPool;

	CTexture& Tex(const std::string& path, bool isColor = true)
	{
		g_texPool.push_back(std::make_unique<CTexture>(path, isColor));
		return *g_texPool.back();
	}

	// ImGui 面板遍历的模型列表
	std::vector<ModelHandle> g_models;
}

std::vector<ModelHandle>& SceneModels()
{
	return g_models;
}

glm::vec3 CandleFlamePosition()
{
	for (const ModelHandle& m : g_models)
	{
		if (m.name == "Candle" && m.group)
		{
			// candle.obj 局部高度 0.281（+Z 朝上），绕 X 转 -90 后变成 +Y
			return m.group->modelPos +
				glm::vec3(0.0f, 0.27f * m.group->modelScale.y, 0.0f);
		}
	}
	return glm::vec3(0.0f);
}

CScene::Group* CandleGroup()
{
	for (const ModelHandle& m : g_models)
	{
		if (m.name == "Candle" && m.group)
			return m.group;
	}
	return nullptr;
}

CScene::Group* BathroomMirrorGroup()
{
	for (const ModelHandle& m : g_models)
	{
		if (m.name == "BathroomMirror" && m.group)
			return m.group;
	}
	return nullptr;
}

MirrorSurface BathroomMirrorSurface()
{
	MirrorSurface s;

	s.center     = glm::vec3(-4.4f, 1.664f, -14.888f);
	s.normal     = glm::vec3(0.0f, 0.0f, 1.0f);
	s.halfWidth  = 0.5247f;
	s.halfHeight = 0.4142f;

	if (CScene::Group* g = BathroomMirrorGroup())
	{
		const glm::vec3 p  = g->modelPos;
		const glm::vec3 sc = g->modelScale;
		s.center     = glm::vec3(p.x, p.y + 0.4142f * sc.y, p.z + 0.0541f * sc.z);
		s.halfWidth  = 0.5247f * sc.x;
		s.halfHeight = 0.4142f * sc.y;
	}
	return s;
}

void SetupScene(CScene& scene)
{
	// ---- 纯色 ----
	const glm::vec3 floorColor(0.35f, 0.32f, 0.28f);
	const glm::vec3 wallColor(0.95f, 0.93f, 0.9f);
	const glm::vec3 ceilingColor(0.32f, 0.20f, 0.12f);

	// ---- 墙壁 / 地板贴图 ----
	// 墙纸
	CTexture& wallpaperTex    = Tex("assets/textures/WallpaperOld_Color.jpg");
	CTexture& wallpaperNormal = Tex("assets/textures/WallpaperOld_NormalGL.jpg", false);
	CTexture& floorTex          = Tex("assets/textures/WoodFloor007_2K-JPG_Color.jpg");
	CTexture& floorNormal       = Tex("assets/textures/WoodFloor007_2K-JPG_NormalGL.jpg", false);
	CTexture& bathroomWallTex = Tex("assets/textures/Tiles010_2K-JPG_Color.jpg");
	CTexture& bathroomWallNormal = Tex("assets/textures/Tiles010_2K-JPG_NormalGL.jpg");
	// 墙裙
	CTexture& wainscotAlbedo  = Tex("assets/models/the_bathroom_free/textures/Wall_Trim_baseColor.png");
	CTexture& wainscotNormal  = Tex("assets/models/the_bathroom_free/textures/Wall_Trim_normal.png", false);
	CTexture& wainscotMR      = Tex("assets/models/the_bathroom_free/textures/Wall_Trim_metallicRoughness.png", false);

	// 带 MR 贴图的材质：metallic/roughness 常数设 1.0，让贴图完全控制
	auto MRMat = [](const CTexture* albedo, const CTexture* normal, const CTexture* mr,
		const glm::vec3& tint = glm::vec3(1.0f), float uScale = 1.0f,
		bool useMetallic = true)
	{
		CMaterial m;
		m.texture        = albedo;
		m.tint           = tint;
		m.uScale         = uScale;
		m.normalMap      = normal;
		m.normalStrength = 1.0f;
		m.metallicRoughnessMap = mr;
		m.roughness      = 1.0f;
		// 不用贴图金属度时，常数也跟着设 0，否则整个材质还是金属
		m.metallic       = useMetallic ? 1.0f : 0.0f;
		m.mrUseMetallic  = useMetallic;
		return m;
	};

	// ---- 材质分组 ----
	auto& gWall        = scene.NewGroup({ &wallpaperTex, glm::vec3(1.0f), 2.5f, &wallpaperNormal, 1.0f });
	auto& gFloor       = scene.NewGroup({ &floorTex,       glm::vec3(1.0f), 0.4f,  &floorNormal,       1.0f });
	auto& gCeiling     = scene.NewGroup({ nullptr, glm::vec3(1.0f), 1.0f });
	auto& gbathroomWall = scene.NewGroup({ &bathroomWallTex, glm::vec3(1.0f), 0.4f, &bathroomWallNormal, 1.0f });

	// ---- 地面 / 天花板 ----
	scene.AddBox(gFloor, -10.0f, -0.5f, 0.0f, 4.0f, 0.0f, 10.0f, floorColor);
	scene.AddBox(gFloor, -10.0f, -0.5f, -16.0f, -5.75f, 0.0f, 0.0f, floorColor);
	scene.AddBox(gFloor, -5.75f, -0.5f, -15.0f, -1.0f, 0.0f, -11.0f, floorColor);

	scene.AddBox(gCeiling, -10.7f, 3.95f, -16.7f, 4.7f, 4.5f, 10.7f, ceilingColor);   // 整体外扩：盖住所有墙顶，并下沉 5cm 插进墙里，杜绝接缝漏光
	scene.AddBox(gCeiling, -10.8f, 4.35f, -16.8f, 4.8f, 5.0f, 10.8f, ceilingColor);   // 第二层天花板：再高一截，和下面那块重叠 0.15 米

	// ---- 厨房 / 餐厅墙 ----
	scene.AddBox(gWall, -10.5f, 0.0f, 10.0f, 0.279f, 4.0f, 10.5f, wallColor);      // 门口左侧
	scene.AddBox(gWall, 1.721f, 0.0f, 10.0f, 4.5f, 4.0f, 10.5f, wallColor);        // 门口右侧
	scene.AddBox(gWall, 0.279f, 3.0f, 10.0f, 1.721f, 4.0f, 10.5f, wallColor);      // 门楣上
	scene.AddBox(gWall, 4.0f, 0.0f, -0.5f, 4.5f, 4.0f, 10.5f, wallColor);          // 东墙
	// 西墙
	scene.AddBox(gWall, -10.5f, 0.0f, -0.5f, -10.0f, 4.0f, 4.25f, wallColor);      // 窗洞左侧
	scene.AddBox(gWall, -10.5f, 0.0f, 5.74f, -10.0f, 4.0f, 10.5f, wallColor);      // 窗洞右侧
	scene.AddBox(gWall, -10.5f, 0.0f, 4.25f, -10.0f, 1.01f, 5.74f, wallColor);     // 窗台下
	scene.AddBox(gWall, -10.5f, 3.4f, 4.25f, -10.0f, 4.0f, 5.74f, wallColor);      // 窗楣上
	scene.AddBox(gWall, -5.75f, 0.0f, -0.5f, 4.5f, 4.0f, 0.0f, wallColor);         // 北墙（走廊开口）

	// ---- 走廊（x ∈ [-10, -5.75]，净宽 4.25 米；入口墙厚 0.25）----
	scene.AddBox(gWall, -10.5f, 0.0f, -16.5f, -10.0f, 4.0f, -13.75f, wallColor);    // 窗洞1 左侧
	scene.AddBox(gWall, -10.5f, 0.0f, -12.25f, -10.0f, 4.0f, -9.0f, wallColor);      // 窗洞1-2 之间
	scene.AddBox(gWall, -10.5f, 0.0f, -7.5f, -10.0f, 4.0f, -4.25f, wallColor);       // 窗洞2-3 之间
	scene.AddBox(gWall, -10.5f, 0.0f, -2.75f, -10.0f, 4.0f, 0.0f, wallColor);        // 窗洞3 右侧
	scene.AddBox(gWall, -10.5f, 0.0f, -13.75f, -10.0f, 1.01f, -12.25f, wallColor);   // 窗洞1 窗台下
	scene.AddBox(gWall, -10.5f, 3.4f, -13.75f, -10.0f, 4.0f, -12.25f, wallColor);    // 窗洞1 窗楣上
	scene.AddBox(gWall, -10.5f, 0.0f, -9.0f, -10.0f, 1.01f, -7.5f, wallColor);       // 窗洞2 窗台下
	scene.AddBox(gWall, -10.5f, 3.4f, -9.0f, -10.0f, 4.0f, -7.5f, wallColor);        // 窗洞2 窗楣上
	scene.AddBox(gWall, -10.5f, 0.0f, -4.25f, -10.0f, 1.01f, -2.75f, wallColor);     // 窗洞3 窗台下
	scene.AddBox(gWall, -10.5f, 3.4f, -4.25f, -10.0f, 4.0f, -2.75f, wallColor);      // 窗洞3 窗楣上

	// 走廊入口
	scene.AddBox(gWall, -10.0f, 0.0f, -0.25f, -8.8f, 4.0f, 0.0f, wallColor);        // 门口左侧
	scene.AddBox(gWall, -7.28f, 0.0f, -0.25f, -5.75f, 4.0f, 0.0f, wallColor);       // 门口右侧
	scene.AddBox(gWall, -8.8f, 3.09f, -0.25f, -7.28f, 4.0f, 0.0f, wallColor);       // 门楣上

	// 尽头墙
	scene.AddBox(gWall, -10.5f, 0.0f, -16.5f, -8.72f, 4.0f, -16.0f, wallColor);     // 门口左侧
	scene.AddBox(gWall, -7.28f, 0.0f, -16.5f, -5.5f, 4.0f, -16.0f, wallColor);      // 门口右侧
	scene.AddBox(gWall, -8.72f, 3.09f, -16.5f, -7.28f, 4.0f, -16.0f, wallColor);    // 门楣上

	scene.AddBox(gWall, -5.75f, 0.0f, -16.0f, -5.5f, 4.0f, -14.0f, wallColor);      // 东墙（北段，厚 0.25）
	scene.AddBox(gWall, -5.75f, 0.0f, -12.4f, -5.5f, 4.0f, 0.0f, wallColor);        // 东墙（南段）
	// 洗手间门口
	scene.AddBox(gWall, -5.75f, 0.0f, -14.0f, -5.5f, 4.0f, -13.92f, wallColor);     // 门框左侧
	scene.AddBox(gWall, -5.75f, 0.0f, -12.48f, -5.5f, 4.0f, -12.4f, wallColor);     // 门框右侧
	scene.AddBox(gWall, -5.75f, 3.09f, -13.92f, -5.5f, 4.0f, -12.48f, wallColor);   // 门楣上

	// 洗手间 
	scene.AddBox(gbathroomWall, -5.60f, 0.0f, -15.5f, -0.65f, 4.0f, -15.0f, wallColor);     // 北墙
	scene.AddBox(gbathroomWall, -1.0f, 0.0f, -15.5f, -0.5f, 4.0f, -10.5f, wallColor);       // 东墙
	scene.AddBox(gbathroomWall, -5.60f, 0.0f, -11.0f, -0.65f, 4.0f, -10.5f, wallColor);     // 南墙

	// 护墙板 
	{
		const float kModelW = 2.727f;   // obj 的原始宽度，横向缩放必须按它算
		const float kModW   = 3.0f;     // 目标单块宽度，只决定分几块
		const float kFull   = 1.70f;    // 全高：0.919 x 1.70 ≈ 1.56 米
		const float kLow    = 1.09f;    // 窗下：1.00 米（窗台在 y = 1.01）

		// start = 墙段起点，along = 沿墙方向，len = 段长，rotY = 让 +Z 朝室内，kH = 高度倍率
		auto placeWainscot = [&](const glm::vec3& start, const glm::vec3& along,
			float len, float rotY, float kH)
		{
			// 至少 1 块
			const int   n  = (int)((len - 1e-4f) / kModW) + 1;
			// 每块实际宽度
			const float w  = len / n;
			// 横向缩放
			const float sx = w / kModelW;

			// 沿法线离墙 8mm：模型的背面正好在 z=0，如果直接放在墙面坐标上，
			// 背面和墙面完全共面，深度值相同，从室内看就是一片闪动的贴图（z-fighting）
			const float kLift = 0.008f;
			const float rad   = glm::radians(rotY);
			const glm::vec3 lift(std::sin(rad) * kLift, 0.0f, std::cos(rad) * kLift);

			for (int i = 0; i < n; ++i)
			{
				const glm::vec3 p = start + along * ((i + 0.5f) * w) + lift;
				scene.AddModel("assets/models/the_bathroom_free/wainscot.obj",
					p, glm::vec3(0.0f, rotY, 0.0f), glm::vec3(sx, kH, 1.0f),
					// Wall_Trim 的 MR 贴图 B 通道整张都是 255（资产导出问题），
					// 只用 G 通道的粗糙度，金属度保持 0
					MRMat(&wainscotAlbedo, &wainscotNormal, &wainscotMR,
						glm::vec3(1.0f), 1.0f, false),
					false);
			}
		};

		// 餐厅
		placeWainscot(glm::vec3(-10.0f, 0.0f, 10.0f), glm::vec3(1, 0, 0), 10.279f, 180.0f, kFull);  // 南墙（门洞左侧）
		placeWainscot(glm::vec3(1.721f, 0.0f, 10.0f), glm::vec3(1, 0, 0), 2.279f, 180.0f, kFull);   // 南墙（门洞右侧）
		placeWainscot(glm::vec3(4.0f, 0.0f, 0.0f), glm::vec3(0, 0, 1), 10.0f, -90.0f, kFull);        // 东墙
		placeWainscot(glm::vec3(-5.75f, 0.0f, 0.0f), glm::vec3(1, 0, 0), 9.75f, 0.0f, kFull);        // 北墙
		// 西墙
		placeWainscot(glm::vec3(-10.0f, 0.0f, 0.0f), glm::vec3(0, 0, 1), 4.25f, 90.0f, kFull);       // 窗南
		placeWainscot(glm::vec3(-10.0f, 0.0f, 4.25f), glm::vec3(0, 0, 1), 1.49f, 90.0f, kLow);       // 窗下
		placeWainscot(glm::vec3(-10.0f, 0.0f, 5.74f), glm::vec3(0, 0, 1), 4.26f, 90.0f, kFull);      // 窗北

		// 走廊西墙：三个窗洞，七段
		placeWainscot(glm::vec3(-10.0f, 0.0f, -16.0f), glm::vec3(0, 0, 1), 2.25f, 90.0f, kFull);     // 尽头到窗1
		placeWainscot(glm::vec3(-10.0f, 0.0f, -13.75f), glm::vec3(0, 0, 1), 1.50f, 90.0f, kLow);     // 窗1 下
		placeWainscot(glm::vec3(-10.0f, 0.0f, -12.25f), glm::vec3(0, 0, 1), 3.25f, 90.0f, kFull);    // 窗1-2 之间
		placeWainscot(glm::vec3(-10.0f, 0.0f, -9.0f), glm::vec3(0, 0, 1), 1.50f, 90.0f, kLow);       // 窗2 下
		placeWainscot(glm::vec3(-10.0f, 0.0f, -7.5f), glm::vec3(0, 0, 1), 3.25f, 90.0f, kFull);      // 窗2-3 之间
		placeWainscot(glm::vec3(-10.0f, 0.0f, -4.25f), glm::vec3(0, 0, 1), 1.50f, 90.0f, kLow);      // 窗3 下
		placeWainscot(glm::vec3(-10.0f, 0.0f, -2.75f), glm::vec3(0, 0, 1), 2.75f, 90.0f, kFull);     // 窗3 到入口

		// 走廊东墙 + 尽头墙（门洞两侧断开）
		placeWainscot(glm::vec3(-5.75f, 0.0f, -16.0f), glm::vec3(0, 0, 1), 2.08f, -90.0f, kFull);    // 洗手间门北侧
		placeWainscot(glm::vec3(-5.75f, 0.0f, -12.48f), glm::vec3(0, 0, 1), 12.48f, -90.0f, kFull);  // 门南侧
		placeWainscot(glm::vec3(-10.0f, 0.0f, -16.0f), glm::vec3(1, 0, 0), 1.28f, 0.0f, kFull);      // 尽头墙左
		placeWainscot(glm::vec3(-7.28f, 0.0f, -16.0f), glm::vec3(1, 0, 0), 1.78f, 0.0f, kFull);      // 尽头墙右
	}

	// ---- 石砌 / 镜面 ----
	// 洗手台与镜子已换成真模型，见文件末尾

	// ---- 模型贴图 ----
	CTexture& tableAlbedo  = Tex("assets/models/table-damaged/textures/Table-Damage_low_albedo.jpeg");
	CTexture& tableNormal  = Tex("assets/models/table-damaged/textures/Table-Damage_low_normal.jpeg", false);

	CTexture& chairAlbedo  = Tex("assets/models/chair/Textures/Blender_2K/T_Chair_BaseColor.jpg");
	CTexture& chairNormal  = Tex("assets/models/chair/Textures/Blender_2K/T_Chair_Normal.png", false);
	// roughness 是单通道灰度图；配套的 Metallic 贴图整张全黑（该资产没有金属部件），
	// 接入时必须关掉 mrUseMetallic，否则 B 通道会被当成金属度
	CTexture& chairRough   = Tex("assets/models/chair/textures/T_Chair_Roughness.png", false);

	CTexture& candleAlbedo = Tex("assets/models/the_bathroom_free/textures/Candle_baseColor.png");
	CTexture& candleNormal = Tex("assets/models/the_bathroom_free/textures/Candle_normal.png", false);

	// Poly Haven 的 ARM 贴图：R=AO, G=Roughness, B=Metallic，
	// 通道布局正好符合 glTF 约定，可以直接当 metallicRoughness 用
	CTexture& cabinetAlbedo = Tex("assets/models/gothic-cabinet/textures/GothicCabinet_01_diff_2k.jpg");
	CTexture& cabinetNormal = Tex("assets/models/gothic-cabinet/textures/GothicCabinet_01_nor_gl_2k.jpg", false);
	CTexture& cabinetMR     = Tex("assets/models/gothic-cabinet/textures/GothicCabinet_01_arm_2k.jpg", false);

	CTexture& barrelAlbedo = Tex("assets/models/barrel/textures/Barrel_01_diff.jpg");
	CTexture& barrelNormal = Tex("assets/models/barrel/textures/Barrel_01_nor.jpg", false);
	CTexture& barrelMR     = Tex("assets/models/barrel/textures/Barrel_01_oxide_grey_arm_2k.jpg", false);

	CTexture& lanternAlbedo = Tex("assets/models/lantern/textures/Lantern_01_brass_diff_2k.jpg");
	CTexture& lanternNormal = Tex("assets/models/lantern/textures/Lantern_01_brass_nor_gl_2k.jpg", false);
	CTexture& lanternMR     = Tex("assets/models/lantern/textures/Lantern_01_brass_arm_2k.jpg", false);

	CTexture& chandelierAlbedo = Tex("assets/models/chandelier/textures/Chandelier_02_diff_2k.jpg");
	CTexture& chandelierNormal = Tex("assets/models/chandelier/textures/Chandelier_02_nor_gl_2k.jpg", false);
	CTexture& chandelierMR     = Tex("assets/models/chandelier/textures/Chandelier_02_arm_2k.jpg", false);

	CTexture& fireplaceAlbedo = Tex("assets/models/fireplace/textures/fireplace_baseColor.jpg");
	CTexture& fireplaceNormal = Tex("assets/models/fireplace/textures/fireplace_normal.jpg", false);
	CTexture& fireplaceMR     = Tex("assets/models/fireplace/textures/fireplace_metallicRoughness.png", false);

	// 相框：木框 + 画布两套材质，拆成两个 obj
	// rough 是单通道灰度图（没有 metallic 数据），接入时必须关掉 mrUseMetallic，
	// 否则 B 通道会被当成金属度，木头画框会变成半金属
	CTexture& frameAlbedo  = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_diff_4k.jpg");
	CTexture& frameNormal  = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_nor_gl_4k.jpg", false);
	CTexture& frameRough   = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_rough_4k.jpg", false);
	CTexture& canvasAlbedo = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_canvas_diff_4k.jpg");
	CTexture& canvasNormal = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_canvas_nor_gl_4k.jpg", false);
	CTexture& canvasRough  = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_canvas_rough_4k.jpg", false);

	CTexture& windowAlbedo = Tex("assets/models/wooden_window/textures/Scene_-_Root_baseColor.png");
	CTexture& windowNormal = Tex("assets/models/wooden_window/textures/Scene_-_Root_normal.png", false);

	CTexture& doorAlbedo = Tex("assets/models/door/textures/Default_baseColor.png");
	CTexture& doorNormal = Tex("assets/models/door/textures/Default_normal.png", false);

	// 浴室
	CTexture& bathtubAlbedo = Tex("assets/models/the_bathroom_free/textures/Bath_baseColor.png");
	CTexture& bathtubNormal = Tex("assets/models/the_bathroom_free/textures/Bath_normal.png", false);
	CTexture& sinkAlbedo    = Tex("assets/models/the_bathroom_free/textures/Bathtube_baseColor.png");
	CTexture& sinkNormal    = Tex("assets/models/the_bathroom_free/textures/Bathtube_normal.png", false);
	CTexture& mirrorAlbedo  = Tex("assets/models/the_bathroom_free/textures/Frames_baseColor.png");
	CTexture& mirrorNormal  = Tex("assets/models/the_bathroom_free/textures/Frames_normal.png", false);
	CTexture& tileAlbedo    = Tex("assets/models/the_bathroom_free/textures/Floor_baseColor.png");
	CTexture& tileNormal    = Tex("assets/models/the_bathroom_free/textures/Floor_normal.png", false);

	// 金属度/粗糙度贴图
	// 必须 isColor = false：这是线性数据，当 sRGB 解码会让粗糙度整体偏亮
	CTexture& candleMR   = Tex("assets/models/the_bathroom_free/textures/Candle_metallicRoughness.png", false);
	CTexture& bathtubMR  = Tex("assets/models/the_bathroom_free/textures/Bath_metallicRoughness.png", false);
	CTexture& sinkMR     = Tex("assets/models/the_bathroom_free/textures/Bathtube_metallicRoughness.png", false);
	CTexture& mirrorMR   = Tex("assets/models/the_bathroom_free/textures/Frames_metallicRoughness.png", false);
	CTexture& tileMR     = Tex("assets/models/the_bathroom_free/textures/Floor_metallicRoughness.png", false);
	CTexture& doorMR     = Tex("assets/models/door/textures/Default_metallicRoughness.png", false);
	CTexture& windowMR   = Tex("assets/models/wooden_window/textures/Scene_-_Root_metallicRoughness.png", false);

	auto reg = [](std::string name, CScene::Group& g)
	{
		g_models.push_back({ std::move(name), &g, g.modelPos, g.modelRotDeg, g.modelScale });
	};

	// 桌子
	scene.AddModel("assets/models/table-damaged/source/Table-Damage_low.obj",
		glm::vec3(-1.7f, -0.25f, 6.5f),
		glm::vec3(0.0f),
		glm::vec3(0.065f),
		{ &tableAlbedo, glm::vec3(1.0f), 1.0f, &tableNormal, 1.0f , 0.0f, 0.65f}); // 最后两个参数为metallic、roughness

	// 蜡烛
	{
		auto& gCandle = scene.AddModel("assets/models/the_bathroom_free/candle.obj",
			glm::vec3(-1.5f, 1.22f, 6.0f),
			glm::vec3(-90.0f, 120.0f, 0.0f),
			glm::vec3(1.5f),
			MRMat(&candleAlbedo, &candleNormal, &candleMR),  // MEMat会把 roughness 和 metallic 自动设成 1.0
			false);
		reg("Candle", gCandle);
	}

	// 椅子 x4
	// 东南
	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-1.3f, 0.05f, 8.0f),
		glm::vec3(3.0f, 90.0f, 90.0f),
		glm::vec3(1.25f),
		MRMat(&chairAlbedo, &chairNormal, &chairRough, glm::vec3(1.0f), 1.0f, false));

	// 西南
	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-2.7f, 0.0f, 8.0f),
		glm::vec3(3.0f, 150.0f, 0.0f),
		glm::vec3(1.25f),
		MRMat(&chairAlbedo, &chairNormal, &chairRough, glm::vec3(1.0f), 1.0f, false));

	// 西北
	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-3.0f, 0.0f, 5.0f),
		glm::vec3(-85.0f, 0.0f, 0.0f),
		glm::vec3(1.25f),
		MRMat(&chairAlbedo, &chairNormal, &chairRough, glm::vec3(1.0f), 1.0f, false));

	// 东北
	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-0.3f, 0.0f, 5.0f),
		glm::vec3(0.0f, 60.0f, 0.0f),
		glm::vec3(1.25f),
		MRMat(&chairAlbedo, &chairNormal, &chairRough, glm::vec3(1.0f), 1.0f, false));

	// 柜子
	scene.AddModel("assets/models/gothic-cabinet/GothicCabinet_01.obj",
		glm::vec3(-7.0f, 0.0f, 9.3f),
		glm::vec3(0.0f, 180.0f, 0.0f),
		glm::vec3(1.5f),
		MRMat(&cabinetAlbedo, &cabinetNormal, &cabinetMR));

	// 铁桶
	{
		auto& g = scene.AddModel("assets/models/barrel/Barrel_01.obj",
			glm::vec3(-8.0f, 0.0f, -15.4f),
			glm::vec3(0.0f),
			glm::vec3(1.5f),
			MRMat(&barrelAlbedo, &barrelNormal, &barrelMR));
		reg("Barrel", g);
	}

	// 铜灯笼
	{
		auto& g = scene.AddModel("assets/models/lantern/Lantern_01.obj",
			glm::vec3(1.2f, 0.0f, 2.9f),
			glm::vec3(0.0f, 20.0f, 0.0f),
			glm::vec3(2.5f),
			MRMat(&lanternAlbedo, &lanternNormal, &lanternMR));
		reg("Lantern", g);
	}

	// 吊灯
	{
		auto& g = scene.AddModel("assets/models/chandelier/Chandelier_02.obj",
			glm::vec3(-2.5f, 2.4f, 4.0f),
			glm::vec3(0.0f),
			glm::vec3(2.6f),
			MRMat(&chandelierAlbedo, &chandelierNormal, &chandelierMR));
		reg("Chandelier", g);
	}

	// 壁炉
	scene.AddModel("assets/models/fireplace/fireplace.obj",
		glm::vec3(2.5f, 0.0f, 0.57f),
		glm::vec3(0.0f, 90.0f, 0.0f),
		glm::vec3(1.5f),
		MRMat(&fireplaceAlbedo, &fireplaceNormal, &fireplaceMR));

	// 相框
	{
		const glm::vec3 framePos(-3.5f, 2.3f, 0.0f);

		// rough 贴图没有 metallic 信息，最后一个参数传 false 只取 G 通道
		scene.AddModel("assets/models/fancy_picture_frame/frame.obj",
			framePos, glm::vec3(0.0f), glm::vec3(3.5f),
			MRMat(&frameAlbedo, &frameNormal, &frameRough, glm::vec3(1.0f), 1.0f, false));

		scene.AddModel("assets/models/fancy_picture_frame/canvas.obj",
			framePos, glm::vec3(0.0f), glm::vec3(3.5f),
			MRMat(&canvasAlbedo, &canvasNormal, &canvasRough, glm::vec3(1.0f), 1.0f, false));
	}

	// 餐厅木窗
	{
		auto& g = scene.AddModel("assets/models/wooden_window/wooden_window.obj",
			glm::vec3(-10.25f, 1.0f, 5.0f),
			glm::vec3(0.0f, -90.0f, 0.0f),
			glm::vec3(1.5f),
			MRMat(&windowAlbedo, &windowNormal, &windowMR));
		g.castShadow = false;
		reg("Window", g);
	}

	// 走廊西墙三扇窗
	for (const float wz : { -13.0f, -8.25f, -3.5f })
	{
		auto& g = scene.AddModel("assets/models/wooden_window/wooden_window.obj",
			glm::vec3(-10.25f, 1.0f, wz),
			glm::vec3(0.0f, -90.0f, 0.0f),
			glm::vec3(1.5f),
			MRMat(&windowAlbedo, &windowNormal, &windowMR));
		g.castShadow = false;
	}

	// 木门（宽 1.4612 / 高 3.099 / 厚 0.2101，原点在门中心）
	// 南墙
	scene.AddModel("assets/models/door/door.obj",
		glm::vec3(1.0f, 1.55f, 10.05f),
		glm::vec3(0.0f, -90.0f, 0.0f),
		glm::vec3(0.015f),
		MRMat(&doorAlbedo, &doorNormal, &doorMR));

	// 走廊入口
	{
		auto& g = scene.AddModel("assets/models/door/door.obj",
			glm::vec3(-6.9f, 1.55f, -0.8f),
			glm::vec3(0.0f, -36.0f, 0.0f),
			glm::vec3(0.015f),
			MRMat(&doorAlbedo, &doorNormal, &doorMR));
		reg("DoorEntry", g);
	}

	// 走廊尽头
	scene.AddModel("assets/models/door/door.obj",
		glm::vec3(-8.0f, 1.55f, -16.0f),
		glm::vec3(0.0f, 90.0f, 0.0f),
		glm::vec3(0.015f),
		MRMat(&doorAlbedo, &doorNormal, &doorMR));

	// 洗手间
	{
		auto& g = scene.AddModel("assets/models/door/door.obj",
			glm::vec3(-5.0f, 1.55f, -12.17f),
			glm::vec3(0.0f, -120.0f, 0.0f),
			glm::vec3(0.015f),
			MRMat(&doorAlbedo, &doorNormal, &doorMR));
		reg("DoorBathroom", g);
	}


	// ---- 洗手间 ----
	{
		auto& g = scene.AddModel("assets/models/the_bathroom_free/sink.obj",
			glm::vec3(-3.9f, 0.0f, -14.175f),
			glm::vec3(0.0f, 180.0f, 0.0f),
			glm::vec3(1.5f),
			MRMat(&sinkAlbedo, &sinkNormal, &sinkMR));
		reg("BathroomSink", g);
	}

	// 镜子
	{
		auto& g = scene.AddModel("assets/models/the_bathroom_free/mirror.obj",
			glm::vec3(-3.9f, 1.35f, -14.914f),
			glm::vec3(0.0f, 180.0f, 0.0f),
			glm::vec3(1.5f),
			MRMat(&mirrorAlbedo, &mirrorNormal, &mirrorMR));
		reg("BathroomMirror", g);
	}

	// 浴缸
	{
		auto& g = scene.AddModel("assets/models/the_bathroom_free/bathtub.obj",
			glm::vec3(-2.8f, 0.0f, -11.698f),
			glm::vec3(0.0f),
			glm::vec3(1.5f),
			MRMat(&bathtubAlbedo, &bathtubNormal, &bathtubMR));
		reg("BathroomTub", g);
	}

	// 地砖
	scene.AddModel("assets/models/the_bathroom_free/floor_tile.obj",
		glm::vec3(-3.25f, 0.002f, -13.0f),
		glm::vec3(0.0f),
		glm::vec3(4.5f / 5.455f, 1.0f, 4.0f / 5.455f),
		MRMat(&tileAlbedo, &tileNormal, &tileMR),
		false);

	// ---- 提交几何 ----
	scene.Build();
	// 4 个 2D 阴影槽，对应灯索引 0~3；蜡烛(索引 4)不投影，不需要槽位
	scene.PrepareShadows(4);
	std::cout << "[SetupScene] collision boxes: " << scene.Boxes().size() << "\n";

	// ---- 灯光 ----
	// 索引 = 阴影槽位，shader 里按 i 硬编码匹配，顺序不能随意调整
	//   0 蜡烛光   点光   → shadowCube0
	//   1 手电筒 聚光   → shadowMap1
	//   2 洗手间 聚光   → shadowMap2
	//   3 月光   平行光 → shadowMap3

	// 0 号：蜡烛光（点光，cubemap 阴影）
	CLight fireplace;
	fireplace.type       = 0;
	fireplace.color      = glm::vec3(1.0f, 0.55f, 0.25f);
	fireplace.intensity  = 3.5f;
	fireplace.position   = glm::vec3(-1.5f, 1.63f, 6.0f);
	fireplace.castShadow = true;
	fireplace.enabled    = false;
	scene.AddLight(fireplace);

	// 1 号：手电筒（聚光，跟随相机）
	CLight flashlight;
	flashlight.type           = 1;
	flashlight.color          = glm::vec3(1.0f);
	flashlight.intensity      = 5.5f;
	flashlight.cutoffDeg      = 5.0f;
	flashlight.outerCutoffDeg = 25.0f;
	flashlight.castShadow     = true;
	flashlight.followCamera   = true;
	flashlight.followOffset   = glm::vec3(0.30f, -0.20f, 0.0f);
	scene.AddLight(flashlight);

	// 2 号：洗手间灯（点光，固定）
	CLight bathroom;
	bathroom.type       = 0;
	bathroom.intensity  = 10.0f;
	bathroom.position   = glm::vec3(-3.10f, 3.76f, -13.0f);
	bathroom.castShadow = true;
	scene.AddLight(bathroom);

	// 3 号：月光（平行光，正交阴影）
	// position 只是正交阴影相机的起点，从场景中心沿 direction 反方向退出去，
	// 保证整栋房子都落在 [near, far] 之间
	CLight moonlight;
	moonlight.type       = 2;
	moonlight.color      = glm::vec3(0.55f, 0.68f, 1.0f);  // 月光冷蓝
	moonlight.intensity  = 5.0f;
	// 从西侧上方斜射（45°），穿过西墙的窗洞落到地面
	moonlight.direction  = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
	// 沿 direction 反方向退到场景外，保证整栋房子落在正交相机的 [near, far] 内
	moonlight.position   = glm::vec3(-24.4f, 14.9f, -3.0f);
	// 正交盒半宽：相机右方向是 +Z，房子 z 跨度 26 米，半宽必须 ≥ 13
	moonlight.range      = 20.0f;
	moonlight.castShadow = true;
	scene.AddLight(moonlight);

}
