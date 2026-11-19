#include "SceneSetup.h"

#include "CTexture.h"

#include <iostream>
#include <memory>
#include <utility>

namespace
{
	// 贴图集中管理：CMaterial 里存的是裸指针，纹理必须活得比场景久，
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

void SetupScene(CScene& scene)
{
	// ---- 纯色 ----
	const glm::vec3 floorColor(0.35f, 0.32f, 0.28f);
	const glm::vec3 wallColor(0.95f, 0.93f, 0.9f);
	const glm::vec3 ceilingColor(0.32f, 0.20f, 0.12f);

	// ---- 墙壁 / 地板贴图 ----
	// 墙纸：从浴室资产包的 Wall_Trim 图集里裁出的无缝花纹（205px 一个周期，取 2 个周期）
	CTexture& wallpaperTex    = Tex("assets/textures/WallpaperOld_Color.jpg");
	CTexture& wallpaperNormal = Tex("assets/textures/WallpaperOld_NormalGL.jpg", false);
	CTexture& floorTex          = Tex("assets/textures/WoodFloor007_2K-JPG_Color.jpg");
	CTexture& floorNormal       = Tex("assets/textures/WoodFloor007_2K-JPG_NormalGL.jpg", false);
	CTexture& bathroomWallTex = Tex("assets/textures/Tiles010_2K-JPG_Color.jpg");
	CTexture& bathroomWallNormal = Tex("assets/textures/Tiles010_2K-JPG_NormalGL.jpg");
	// 墙裙：资产包墙板原图集，UV 是墙板自带的，不能用世界 UV 平铺
	CTexture& wainscotAlbedo  = Tex("assets/models/the_bathroom_free/textures/Wall_Trim_baseColor.png");
	CTexture& wainscotNormal  = Tex("assets/models/the_bathroom_free/textures/Wall_Trim_normal.png", false);

	// ---- 材质分组 ----
	// 餐厅 + 走廊墙面：墙纸，贴图含 2 个周期 = 0.273 米，2.5 表示每米重复 2.5 次
	auto& gWall        = scene.NewGroup({ &wallpaperTex, glm::vec3(1.0f), 2.5f, &wallpaperNormal, 1.0f });
	auto& gFloor       = scene.NewGroup({ &floorTex,       glm::vec3(1.0f), 0.4f,  &floorNormal,       1.0f });
	auto& gCeiling     = scene.NewGroup({ nullptr, glm::vec3(1.0f), 1.0f });
	auto& gbathroomWall = scene.NewGroup({ &bathroomWallTex, glm::vec3(1.0f), 0.4f, &bathroomWallNormal, 1.0f });

	// ---- 地面 / 天花板 ----
	scene.AddBox(gFloor, -10.0f, -0.5f, 0.0f, 4.0f, 0.0f, 10.0f, floorColor);
	scene.AddBox(gFloor, -10.0f, -0.5f, -16.0f, -5.75f, 0.0f, 0.0f, floorColor);
	scene.AddBox(gFloor, -5.75f, -0.5f, -15.0f, -1.0f, 0.0f, -11.0f, floorColor);

	scene.AddBox(gCeiling, -10.0f, 4.0f, -0.5f, 4.5f, 4.5f, 10.0f, ceilingColor);
	scene.AddBox(gCeiling, -10.0f, 4.0f, -16.0f, -5.75f, 4.5f, 0.0f, ceilingColor);
	scene.AddBox(gCeiling, -5.75f, 4.0f, -15.5f, -1.0f, 4.5f, -10.5f, ceilingColor);

	// ---- 厨房 / 餐厅墙 ----
	// 南墙（在 x ∈ [0.279, 1.721]、y ∈ [0, 2.840] 处开门口，对应木门）
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
	// 西墙三个窗洞：z ∈ [-13.75,-12.25] / [-9.0,-7.5] / [-4.25,-2.75]，洞口 y ∈ [1.01, 3.5]
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
	// 洗手间门口（z ∈ [-13.92, -12.48]）
	scene.AddBox(gWall, -5.75f, 0.0f, -14.0f, -5.5f, 4.0f, -13.92f, wallColor);     // 门框左侧
	scene.AddBox(gWall, -5.75f, 0.0f, -12.48f, -5.5f, 4.0f, -12.4f, wallColor);     // 门框右侧
	scene.AddBox(gWall, -5.75f, 3.09f, -13.92f, -5.5f, 4.0f, -12.48f, wallColor);   // 门楣上

	// 洗手间 
	scene.AddBox(gbathroomWall, -5.60f, 0.0f, -15.5f, -0.65f, 4.0f, -15.0f, wallColor);     // 北墙
	scene.AddBox(gbathroomWall, -1.0f, 0.0f, -15.5f, -0.5f, 4.0f, -10.5f, wallColor);       // 东墙
	scene.AddBox(gbathroomWall, -5.60f, 0.0f, -11.0f, -0.65f, 4.0f, -10.5f, wallColor);     // 南墙

	// 护墙板 
	// wainscot.obj 是从资产包 Wall_Clear_long 里切出的腰线以下部分：2.727 x 0.919 米、厚 5 厘米，
	// 背面在 z=0、正面朝 +Z。有窗的墙按窗洞拆段：窗间全高，窗下压到窗台以下。
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

			for (int i = 0; i < n; ++i)
			{
				const glm::vec3 p = start + along * ((i + 0.5f) * w);
				scene.AddModel("assets/models/the_bathroom_free/wainscot.obj",
					p, glm::vec3(0.0f, rotY, 0.0f), glm::vec3(sx, kH, 1.0f),
					{ &wainscotAlbedo, glm::vec3(1.0f), 1.0f, &wainscotNormal, 1.0f },
					false);
			}
		};

		// 餐厅
		placeWainscot(glm::vec3(-10.0f, 0.0f, 10.0f), glm::vec3(1, 0, 0), 10.279f, 180.0f, kFull);  // 南墙（门洞左侧）
		placeWainscot(glm::vec3(1.721f, 0.0f, 10.0f), glm::vec3(1, 0, 0), 2.279f, 180.0f, kFull);   // 南墙（门洞右侧）
		placeWainscot(glm::vec3(4.0f, 0.0f, 0.0f), glm::vec3(0, 0, 1), 10.0f, -90.0f, kFull);        // 东墙
		placeWainscot(glm::vec3(-5.75f, 0.0f, 0.0f), glm::vec3(1, 0, 0), 9.75f, 0.0f, kFull);        // 北墙
		// 西墙：窗洞 z ∈ [4.25, 5.74]，三段绕开
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
		placeWainscot(glm::vec3(-5.5f, 0.0f, -16.0f), glm::vec3(0, 0, 1), 2.08f, -90.0f, kFull);     // 洗手间门北侧
		placeWainscot(glm::vec3(-5.5f, 0.0f, -12.48f), glm::vec3(0, 0, 1), 12.48f, -90.0f, kFull);   // 门南侧
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

	CTexture& cabinetAlbedo = Tex("assets/models/gothic-cabinet/textures/GothicCabinet_01_diff_2k.jpg");
	CTexture& cabinetNormal = Tex("assets/models/gothic-cabinet/textures/GothicCabinet_01_nor_gl_2k.jpg", false);

	CTexture& barrelAlbedo = Tex("assets/models/barrel/textures/Barrel_01_diff.jpg");
	CTexture& barrelNormal = Tex("assets/models/barrel/textures/Barrel_01_nor.jpg", false);

	CTexture& lanternAlbedo = Tex("assets/models/lantern/textures/Lantern_01_brass_diff_2k.jpg");
	CTexture& lanternNormal = Tex("assets/models/lantern/textures/Lantern_01_brass_nor_gl_2k.jpg", false);

	CTexture& chandelierAlbedo = Tex("assets/models/chandelier/textures/Chandelier_02_diff_2k.jpg");
	CTexture& chandelierNormal = Tex("assets/models/chandelier/textures/Chandelier_02_nor_gl_2k.jpg", false);

	CTexture& fireplaceAlbedo = Tex("assets/models/fireplace/textures/fireplace_baseColor.jpg");
	CTexture& fireplaceNormal = Tex("assets/models/fireplace/textures/fireplace_normal.jpg", false);

	// 相框：木框 + 画布两套材质，拆成两个 obj
	CTexture& frameAlbedo  = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_diff_4k.jpg");
	CTexture& frameNormal  = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_nor_gl_4k.jpg", false);
	CTexture& canvasAlbedo = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_canvas_diff_4k.jpg");
	CTexture& canvasNormal = Tex("assets/models/fancy_picture_frame/textures/fancy_picture_frame_01_canvas_nor_gl_4k.jpg", false);

	CTexture& windowAlbedo = Tex("assets/models/wooden_window/textures/Scene_-_Root_baseColor.png");
	CTexture& windowNormal = Tex("assets/models/wooden_window/textures/Scene_-_Root_normal.png", false);

	CTexture& doorAlbedo = Tex("assets/models/door/textures/Default_baseColor.png");
	CTexture& doorNormal = Tex("assets/models/door/textures/Default_normal.png", false);

	// 浴室资产包（Sketchfab, CC BY 4.0, by Evan）：浴缸 / 洗手台 / 镜子
	CTexture& bathtubAlbedo = Tex("assets/models/the_bathroom_free/textures/Bath_baseColor.png");
	CTexture& bathtubNormal = Tex("assets/models/the_bathroom_free/textures/Bath_normal.png", false);
	CTexture& sinkAlbedo    = Tex("assets/models/the_bathroom_free/textures/Bathtube_baseColor.png");
	CTexture& sinkNormal    = Tex("assets/models/the_bathroom_free/textures/Bathtube_normal.png", false);
	CTexture& mirrorAlbedo  = Tex("assets/models/the_bathroom_free/textures/Frames_baseColor.png");
	CTexture& mirrorNormal  = Tex("assets/models/the_bathroom_free/textures/Frames_normal.png", false);
	CTexture& tileAlbedo    = Tex("assets/models/the_bathroom_free/textures/Floor_baseColor.png");
	CTexture& tileNormal    = Tex("assets/models/the_bathroom_free/textures/Floor_normal.png", false);

	// ---- 可编辑模型表 ----
	auto reg = [](std::string name, CScene::Group& g)
	{
		g_models.push_back({ std::move(name), &g, g.modelPos, g.modelRotDeg, g.modelScale });
	};

	// 桌子：原模型约 52 x 22 x 28 单位，缩到约 1.3 x 0.57 x 0.7 米
	scene.AddModel("assets/models/table-damaged/source/Table-Damage_low.obj",
		glm::vec3(-1.7f, -0.25f, 6.5f),
		glm::vec3(0.0f),
		glm::vec3(0.065f),
		{ &tableAlbedo, glm::vec3(1.0f), 1.0f, &tableNormal, 1.0f });

	// 椅子 x4
	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-1.3f, 0.05f, 8.0f),
		glm::vec3(3.0f, 90.0f, 90.0f),
		glm::vec3(1.25f),
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-2.7f, 0.0f, 8.0f),
		glm::vec3(3.0f, 150.0f, 0.0f),
		glm::vec3(1.25f),
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-3.0f, 0.0f, 5.0f),
		glm::vec3(-85.0f, 0.0f, 0.0f),
		glm::vec3(1.25f),
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	scene.AddModel("assets/models/chair/SM_Chair.obj",
		glm::vec3(-1.3f, 0.0f, 4.5f),
		glm::vec3(0.0f, 60.0f, 0.0f),
		glm::vec3(1.25f),
		{ &chairAlbedo, glm::vec3(1.0f), 1.0f, &chairNormal, 1.0f });

	// 柜子
	scene.AddModel("assets/models/gothic-cabinet/GothicCabinet_01.obj",
		glm::vec3(-7.0f, 0.0f, 9.3f),
		glm::vec3(0.0f, 180.0f, 0.0f),
		glm::vec3(1.5f),
		{ &cabinetAlbedo, glm::vec3(1.0f), 1.0f, &cabinetNormal, 1.0f });

	// 铁桶
	{
		auto& g = scene.AddModel("assets/models/barrel/Barrel_01.obj",
			glm::vec3(-8.0f, 0.0f, -15.4f),
			glm::vec3(0.0f),
			glm::vec3(1.5f),
			{ &barrelAlbedo, glm::vec3(1.0f), 1.0f, &barrelNormal, 1.0f });
		reg("Barrel", g);
	}

	// 铜灯笼：原始 0.12 x 0.29 x 0.10，放大 2.5 倍
	{
		auto& g = scene.AddModel("assets/models/lantern/Lantern_01.obj",
			glm::vec3(1.2f, 0.0f, 2.9f),
			glm::vec3(0.0f, 20.0f, 0.0f),
			glm::vec3(2.5f),
			{ &lanternAlbedo, glm::vec3(1.0f), 1.0f, &lanternNormal, 1.0f });
		reg("Lantern", g);
	}

	// 吊灯：原始 0.68 x 0.85 x 0.62，最高点 y = 0.908
	{
		auto& g = scene.AddModel("assets/models/chandelier/Chandelier_02.obj",
			glm::vec3(-2.5f, 2.4f, 4.0f),
			glm::vec3(0.0f),
			glm::vec3(2.6f),
			{ &chandelierAlbedo, glm::vec3(1.0f), 1.0f, &chandelierNormal, 1.0f });
		reg("Chandelier", g);
	}

	// 壁炉
	scene.AddModel("assets/models/fireplace/fireplace.obj",
		glm::vec3(2.5f, 0.0f, 0.57f),
		glm::vec3(0.0f, 90.0f, 0.0f),
		glm::vec3(1.5f),
		{ &fireplaceAlbedo, glm::vec3(1.0f), 1.0f, &fireplaceNormal, 1.0f });

	// 相框（木框 + 画布，位置一致）
	{
		const glm::vec3 framePos(-3.5f, 2.3f, 0.0f);

		scene.AddModel("assets/models/fancy_picture_frame/frame.obj",
			framePos, glm::vec3(0.0f), glm::vec3(3.5f),
			{ &frameAlbedo, glm::vec3(1.0f), 1.0f, &frameNormal, 1.0f });

		scene.AddModel("assets/models/fancy_picture_frame/canvas.obj",
			framePos, glm::vec3(0.0f), glm::vec3(3.5f),
			{ &canvasAlbedo, glm::vec3(1.0f), 1.0f, &canvasNormal, 1.0f });
	}

	// 餐厅木窗（castShadow = false：玻璃不写深度图，月光才能从窗洞透进来）
	{
		auto& g = scene.AddModel("assets/models/wooden_window/wooden_window.obj",
			glm::vec3(-10.25f, 1.0f, 5.0f),
			glm::vec3(0.0f, -90.0f, 0.0f),
			glm::vec3(1.5f),
			{ &windowAlbedo, glm::vec3(1.0f), 1.0f, &windowNormal, 1.0f });
		g.castShadow = false;
		reg("Window", g);
	}

	// 走廊西墙三扇窗（与餐厅窗同尺寸，只改 z）
	for (const float wz : { -13.0f, -8.25f, -3.5f })
	{
		auto& g = scene.AddModel("assets/models/wooden_window/wooden_window.obj",
			glm::vec3(-10.25f, 1.0f, wz),
			glm::vec3(0.0f, -90.0f, 0.0f),
			glm::vec3(1.5f),
			{ &windowAlbedo, glm::vec3(1.0f), 1.0f, &windowNormal, 1.0f });
		g.castShadow = false;
	}

	// 木门（宽 1.4612 / 高 3.099 / 厚 0.2101，原点在门中心）
	// A. 南墙
	scene.AddModel("assets/models/door/door.obj",
		glm::vec3(1.0f, 1.55f, 10.05f),
		glm::vec3(0.0f, -90.0f, 0.0f),
		glm::vec3(0.015f),
		{ &doorAlbedo, glm::vec3(1.0f), 1.0f, &doorNormal, 1.0f });

	// B. 走廊入口
	{
		auto& g = scene.AddModel("assets/models/door/door.obj",
			glm::vec3(-6.9f, 1.55f, -0.8f),
			glm::vec3(0.0f, -36.0f, 0.0f),
			glm::vec3(0.015f),
			{ &doorAlbedo, glm::vec3(1.0f), 1.0f, &doorNormal, 1.0f });
		reg("DoorEntry", g);
	}

	// C. 走廊尽头
	scene.AddModel("assets/models/door/door.obj",
		glm::vec3(-8.0f, 1.55f, -16.0f),
		glm::vec3(0.0f, 90.0f, 0.0f),
		glm::vec3(0.015f),
		{ &doorAlbedo, glm::vec3(1.0f), 1.0f, &doorNormal, 1.0f });

	// D. 洗手间
	{
		auto& g = scene.AddModel("assets/models/door/door.obj",
			glm::vec3(-5.0f, 1.55f, -12.17f),
			glm::vec3(0.0f, -120.0f, 0.0f),
			glm::vec3(0.015f),
			{ &doorAlbedo, glm::vec3(1.0f), 1.0f, &doorNormal, 1.0f });
		reg("DoorBathroom", g);
	}


	// ---- 洗手间 ----
	// 洗手台 1.10 x 1.10 x 0.78：原始资产里背面朝 +Z 贴 z≈10.1 的墙，转 180° 让它贴北墙 z=-15
	{
		auto& g = scene.AddModel("assets/models/the_bathroom_free/sink.obj",
			glm::vec3(-4.4f, 0.0f, -14.608f),
			glm::vec3(0.0f, 180.0f, 0.0f),
			glm::vec3(1.5f),
			{ &sinkAlbedo, glm::vec3(1.0f), 1.0f, &sinkNormal, 1.0f });
		reg("BathroomSink", g);
	}

	// 镜子 1.05 x 0.83 x 0.11：挂在洗手台正上方
	{
		auto& g = scene.AddModel("assets/models/the_bathroom_free/mirror.obj",
			glm::vec3(-4.4f, 1.865f, -14.946f),
			glm::vec3(0.0f, 180.0f, 0.0f),
			glm::vec3(1.5f),
			{ &mirrorAlbedo, glm::vec3(1.0f), 1.0f, &mirrorNormal, 1.0f });
		reg("BathroomMirror", g);
	}

	// 浴缸 1.60 x 0.93 x 0.77：长边沿 X 贴南墙 z=-11，放在南墙东段避开斜开的门
	{
		auto& g = scene.AddModel("assets/models/the_bathroom_free/bathtub.obj",
			glm::vec3(-2.7f, 0.0f, -11.42f),
			glm::vec3(0.0f),
			glm::vec3(1.5f),
			{ &bathtubAlbedo, glm::vec3(1.0f), 1.0f, &bathtubNormal, 1.0f });
		reg("BathroomTub", g);
	}

	// 地砖：资产包的 Floor tile 5.455 x 5.455 米，缩放到 4.75 x 4.00 正好铺满洗手间
	scene.AddModel("assets/models/the_bathroom_free/floor_tile.obj",
		glm::vec3(-3.375f, 0.002f, -13.0f),
		glm::vec3(0.0f),
		glm::vec3(0.8708f, 1.0f, 0.7333f),
		{ &tileAlbedo, glm::vec3(1.0f), 1.0f, &tileNormal, 1.0f },
		false);

	// ---- 提交几何 ----
	scene.Build();
	// 4 个 2D 阴影槽，对应灯索引 0~3；蜡烛(索引 4)不投影，不需要槽位
	scene.PrepareShadows(4);
	std::cout << "[SetupScene] collision boxes: " << scene.Boxes().size() << "\n";

	// ---- 灯光 ----
	// 索引 = 阴影槽位，shader 里按 i 硬编码匹配，顺序不能随意调整
	//   0 壁炉   点光   → shadowCube0
	//   1 手电筒 聚光   → shadowMap1
	//   2 洗手间 聚光   → shadowMap2
	//   3 月光   平行光 → shadowMap3

	// 0 号：壁炉（点光，cubemap 阴影）
	CLight fireplace;
	fireplace.type       = 0;
	fireplace.color      = glm::vec3(1.0f, 0.55f, 0.25f);
	fireplace.intensity  = 0.0f;
	fireplace.position   = glm::vec3(2.50f, 0.70f, 0.90f);
	fireplace.castShadow = true;
	scene.AddLight(fireplace);

	// 1 号：手电筒（聚光，跟随相机）
	CLight flashlight;
	flashlight.type           = 1;
	flashlight.color          = glm::vec3(1.0f);
	flashlight.intensity      = 2.5f;
	flashlight.cutoffDeg      = 10.0f;
	flashlight.outerCutoffDeg = 25.0f;
	flashlight.castShadow     = true;
	flashlight.followCamera   = true;
	scene.AddLight(flashlight);

	// 2 号：洗手间灯（聚光，固定）
	CLight bathroom;
	bathroom.type           = 1;
	bathroom.intensity      = 10.0f;
	bathroom.position       = glm::vec3(-4.0f, 3.0f, -13.0f);
	bathroom.direction      = glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f));
	bathroom.cutoffDeg      = 14.0f;
	bathroom.outerCutoffDeg = 30.0f;
	bathroom.castShadow     = true;
	scene.AddLight(bathroom);

	// 3 号：月光（平行光，正交阴影）
	// position 只是正交阴影相机的起点，从场景中心沿 direction 反方向退出去，
	// 保证整栋房子都落在 [near, far] 之间
	CLight moonlight;
	moonlight.type       = 2;
	moonlight.color      = glm::vec3(0.55f, 0.68f, 1.0f);  // 月光冷蓝
	moonlight.intensity  = 0.6f;
	moonlight.direction  = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
	moonlight.position   = glm::vec3(-24.4f, 14.9f, -3.0f);
	// 正交盒半宽
	moonlight.range      = 20.0f;
	moonlight.castShadow = true;
	scene.AddLight(moonlight);

}
