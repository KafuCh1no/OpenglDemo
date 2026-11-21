#version 330 core

layout (location = 0) in vec3 aPos;      // 单位方片：x/y ∈ [-1,1]
layout (location = 1) in vec2 aUV;

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 model;
uniform mat4 uMirrorVP;   // 镜面相机的 view * projection

out vec2 vUV;
out vec3 vProj;           // 镜面相机下的裁剪坐标（xy/w 才是采样坐标）

void main()
{
    vUV = aUV;

    vec4 world = model * vec4(aPos, 1.0);

    // 主相机：决定镜面片画在屏幕哪个位置
    gl_Position = uProjection * uView * world;

    // 镜面相机：决定这块镜面像素该去反射图里哪个位置取色
    vProj = (uMirrorVP * world).xyw;
}
