#version 330 core
// 画一个立方体，把方向传给 fragment 着色器。
// 环境 cubemap 的六个面、辐照度卷积、预过滤都用同一个顶点着色器。
layout (location = 0) in vec3 aPos;

out vec3 vLocalPos;

uniform mat4 uProjection;
uniform mat4 uView;

void main()
{
    vLocalPos   = aPos;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
