#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "CScene.h"

// 一个可被 ImGui 拖动调参的模型
struct ModelHandle
{
	std::string    name;
	CScene::Group* group = nullptr;
	glm::vec3      initPos{ 0.0f };
	glm::vec3      initRot{ 0.0f };
	glm::vec3      initScale{ 1.0f };
};

// 搭建整个场景，必须在 GL 上下文创建之后调用，且只调用一次。
void SetupScene(CScene& scene);

// ImGui 面板用的可编辑模型列表（SetupScene 之后有效）
std::vector<ModelHandle>& SceneModels();

// 蜡烛火焰根部（世界坐标），每帧读取，跟随 ImGui 拖动
glm::vec3 CandleFlamePosition();

// 蜡烛模型所在的分组（交互描边要用它的 mesh）
CScene::Group* CandleGroup();

// 洗手间镜子的玻璃面（平面反射用）：中心点、朝向房间的法线、半宽/半高。
// 会跟随 ImGui 里拖动过的 "BathroomMirror" 模型位置
struct MirrorSurface
{
	glm::vec3 center{ 0.0f };
	glm::vec3 normal{ 0.0f, 0.0f, 1.0f };
	float     halfWidth  = 0.7f;
	float     halfHeight = 0.54f;
};
MirrorSurface BathroomMirrorSurface();

// 镜子模型所在的分组（反射 Pass 里要临时藏掉它，否则镜子里会看到镜子自己）
CScene::Group* BathroomMirrorGroup();
