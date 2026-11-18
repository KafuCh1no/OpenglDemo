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
