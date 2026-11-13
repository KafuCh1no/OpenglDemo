#include "CCollision.h"

// 玩家与一个物体的碰撞检测
bool CCollision::IsCollision(const glm::vec3& eyePos, float radius, float height, const Box& box)
{
    // 玩家竖直范围：脚底(eyePos.y - height) 到 头顶略高(eyePos.y + 0.1)
    const float playerMinY = eyePos.y - height;
    const float playerMaxY = eyePos.y + 0.1f;

    // 水平方向：把盒子向外膨胀 radius，玩家当成一个点
    // 竖直方向：玩家当作一段区间，和盒子的 y 区间求重叠
    return eyePos.x > box.min.x - radius && eyePos.x < box.max.x + radius
        && eyePos.z > box.min.z - radius && eyePos.z < box.max.z + radius
        && playerMaxY > box.min.y && playerMinY < box.max.y;
}

// 玩家与所有物体的碰撞检测
bool CCollision::IsCollision(const glm::vec3& eyePos, float radius, float height,
                             const std::vector<Box>& boxes)
{
    for (const auto& box : boxes)
    {
        if (IsCollision(eyePos, radius, height, box))
            return true;
    }
    return false;
}

glm::vec3 CCollision::MoveWithCollision(const glm::vec3& eyePos, const glm::vec3& delta,
                                        float radius, float height, const std::vector<Box>& boxes)
{
    // 出生点就在盒子里（或被卡住）：直接放行，否则永远动不了
    if (IsCollision(eyePos, radius, height, boxes))
        return eyePos + delta;

    glm::vec3 result = eyePos;

    // ---- X 轴：单独试 ----
    if (delta.x != 0.0f)
    {
        glm::vec3 test = result;
        test.x += delta.x;
        if (!IsCollision(test, radius, height, boxes))
            result.x = test.x;      // 没撞 → 接受这次位移
        // 撞了就保持 result.x 不变 → 这个轴被挡掉
    }

    // ---- Z 轴：用已经修正过的 result 再试 ----
    // 分两次是关键：X 被挡住时 Z 依然能通过，于是产生沿墙滑动的效果
    if (delta.z != 0.0f)
    {
        glm::vec3 test = result;
        test.z += delta.z;
        if (!IsCollision(test, radius, height, boxes))
            result.z = test.z;
    }

    return result;
}
