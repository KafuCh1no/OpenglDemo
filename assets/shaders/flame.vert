#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aUV;

uniform mat4  uViewProj;
uniform vec3  uBasePos;
uniform float uTime;
uniform float uScale;
uniform float uHeight;
uniform float uRadius;

out vec2  vUV;
out float vH;

void main()
{
    // 模型是单位尺寸：y 0..1 表示从根到尖
    float h = clamp(aPos.y, 0.0, 1.0);

    // 越靠上摆动越大，底部几乎不动
    float wobble = h * h;
    vec3 p = aPos;
    p.x += sin(uTime * 6.1 + aPos.y * 40.0) * 0.10 * wobble;
    p.z += cos(uTime * 5.3 + aPos.y * 35.0) * 0.10 * wobble;

    // 单位模型 -> 实际尺寸（半径 / 高度分开缩放）
    vec3 local = vec3(p.x * uRadius, p.y * uHeight, p.z * uRadius) * uScale;
    vec3 world = uBasePos + local;

    vUV = aUV;
    vH  = h;

    gl_Position = uViewProj * vec4(world, 1.0);
}
