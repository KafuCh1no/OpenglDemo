#pragma once

#include <glm/glm.hpp>
#include <vector>

/** 轴对齐包围盒（AABB） */
struct Box
{
    glm::vec3 min;
    glm::vec3 max;
};

/**
 * 碰撞工具类：全是纯数学，不需要创建对象（全部 static）
 * 数据来自 appendBoxMesh 里顺手记录的 Box，而不是渲染用的 verts
 */
class CCollision
{
public:
    /**
     * 玩家（竖直圆柱）与盒子是否相交
     * @param eyePos  相机位置（眼睛）
     * @param radius  身体半径
     * @param height  眼睛到脚底的高度（约 1.7）
     */
    static bool IsCollision(const glm::vec3& eyePos, float radius, float height, const Box& box);

    /** 是否碰到任意一个盒子 */
    static bool IsCollision(const glm::vec3& eyePos, float radius, float height,
                            const std::vector<Box>& boxes);

    /** 逐轴移动：撞了只取消该轴分量，从而能沿墙滑动 */
    static glm::vec3 MoveWithCollision(const glm::vec3& eyePos, const glm::vec3& delta,
                                       float radius, float height, const std::vector<Box>& boxes);
};
