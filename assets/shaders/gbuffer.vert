#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aUV;

out vec3 WorldPos;
out vec3 Normal;
out vec3 ObjectColor;
out vec2 TexCoords;
out vec2 vWorldUV;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform bool uUseWorldUV;

// 几何 Pass：只算位置和法线，光照留给 deferred.frag
void main()
{
    vec4 world4 = model * vec4(aPos, 1.0);

    WorldPos    = world4.xyz;
    // 非等比缩放时必须用逆转置，否则法线不再垂直于表面
    Normal      = mat3(transpose(inverse(model))) * aNormal;
    ObjectColor = aColor;
    TexCoords   = aUV;

    // 世界空间 UV：按面法线选投影平面（单位：米）
    vec3 an = abs(Normal);
    if (an.x >= an.y && an.x >= an.z)
        vWorldUV = WorldPos.zy;
    else if (an.y >= an.z)
        vWorldUV = WorldPos.xz;
    else
        vWorldUV = WorldPos.xy;

    gl_Position = projection * view * world4;
}
