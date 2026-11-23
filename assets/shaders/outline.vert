#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4  uViewProj;    // projection * view
uniform mat4  model;
uniform float uThickness;   // 沿法线外扩的距离（模型空间）

out float vWorldY;          // 世界空间高度：片元里用来做"从下往上"的裁剪

// 法线外扩描边：把顶点沿法线推出去，配合"只画背面"，
// 从外面看就是物体边缘露出的一圈轮廓
void main()
{
    vec4 world4 = model * vec4(aPos + aNormal * uThickness, 1.0);
    vWorldY = world4.y;

    gl_Position = uViewProj * world4;
}
