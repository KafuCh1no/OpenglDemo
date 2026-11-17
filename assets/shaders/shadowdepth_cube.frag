#version 330 core
in vec3 vWorldPos;

uniform vec3  lightPos;
uniform float farPlane;

void main()
{
    // 点光阴影存「到光源的线性距离」，不是 NDC 深度
    // 因为 6 个面的投影方向不同，比 NDC 深度容易在面交界处出错
    // 归一化到 [0,1] 后直接写进深度缓冲，主 Pass 用同样的公式比较
    gl_FragDepth = length(vWorldPos - lightPos) / farPlane;
}
