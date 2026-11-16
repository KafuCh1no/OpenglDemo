#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aUV;

out vec3 WorldPos;
out vec3 Normal;
out vec3 ObjectColor;
out vec2 TexCoords;
out vec2 vWorldUV;        // 世界空间 UV（单位：米）
out vec4 vLightSpacePos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;             // 局部 -> 世界；盒子传单位矩阵
uniform mat4 lightSpaceMatrix;
uniform bool uUseWorldUV;       // true = 墙面用世界 UV，false = 模型用自带 UV

void main()
{
    vec4 world4 = model * vec4(aPos, 1.0);

    WorldPos    = world4.xyz;
    // 非等比缩放时，法线必须用 model 的逆转置矩阵变换
    Normal      = mat3(transpose(inverse(model))) * aNormal;
    ObjectColor = aColor;
    TexCoords   = aUV;

    // 世界空间 UV：按面法线选投影平面，坐标单位是米
    // 只有 uUseWorldUV = true（墙面）时才在 frag 里使用
    vec3 an = abs(Normal);
    if (an.x >= an.y && an.x >= an.z)
        vWorldUV = WorldPos.zy;      // 朝 X 的面：用 Z / Y
    else if (an.y >= an.z)
        vWorldUV = WorldPos.xz;      // 朝 Y 的面：用 X / Z
    else
        vWorldUV = WorldPos.xy;      // 朝 Z 的面：用 X / Y

    vLightSpacePos = lightSpaceMatrix * world4;
    gl_Position    = projection * view * world4;
}
