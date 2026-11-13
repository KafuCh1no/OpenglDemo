#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aUV;

out vec3 WorldPos;
out vec3 Normal;
out vec3 ObjectColor;
out vec2 TexCoords;
out vec4 vLightSpacePos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 lightSpaceMatrix;

void main()
{
    // 场景顶点直接用世界坐标烘焙（main.cpp 里 addBox 已算好），不需要 model 矩阵
    WorldPos     = aPos;
    Normal       = aNormal;
    ObjectColor  = aColor;
    TexCoords    = aUV;
    vLightSpacePos = lightSpaceMatrix * vec4(aPos, 1.0);  // 世界坐标 → 灯裁剪空间
    gl_Position = projection * view * vec4(aPos, 1.0);
}
