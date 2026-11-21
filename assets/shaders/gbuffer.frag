#version 330 core
// G-buffer：把每个像素的“几何事实”存下来，光照全部推迟到 deferred.frag
layout (location = 0) out vec4 gAlbedo;    // rgb = 反照率, a = 1（0 表示天空/无几何，着色时直接跳过）
layout (location = 1) out vec4 gNormal;    // 世界空间法线
layout (location = 2) out vec4 gPosition;  // 世界空间位置

in vec3 WorldPos;
in vec3 Normal;
in vec3 ObjectColor;
in vec2 TexCoords;
in vec2 vWorldUV;

uniform sampler2D albedoMap;
uniform bool      uHasTexture;
uniform float     uScale;         // UV 平铺密度
uniform vec3      tint;           // 颜色倾向
uniform sampler2D normalMap;
uniform bool      uHasNormalMap;
uniform float     uNormalStrength;
uniform bool      uUseWorldUV;
// PBR 材质参数：写进法线图/位置图的 alpha 通道，不为它们多开附件
uniform float     uMetallic;
uniform float     uRoughness;

// 由切线/副切线/法线组成“表面局部坐标系 -> 世界坐标系”的矩阵
mat3 BuildTBN(vec3 N, vec3 worldPos, vec2 uv)
{
    vec3 dp1 = dFdx(worldPos);
    vec3 dp2 = dFdy(worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);

    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

void main()
{
    // 墙面：世界 UV；模型：自带 UV
    vec2 uv = uUseWorldUV ? (vWorldUV * uScale) : (TexCoords * uScale);

    vec3 N = normalize(Normal);

    if (uHasNormalMap)
    {
        vec3 tn = texture(normalMap, uv).rgb * 2.0 - 1.0;
        tn.xy *= uNormalStrength;
        N = normalize(BuildTBN(N, WorldPos, uv) * tn);
    }

    // albedo：顶点色 × tint，有贴图再乘贴图（必须保持线性，不能在这里 gamma 编码）
    vec3 albedo = ObjectColor * tint;
    if (uHasTexture)
        albedo *= texture(albedoMap, uv).rgb;

    // alpha 通道：albedo 存几何标记（1=有几何），normal 存 roughness，position 存 metallic
    gAlbedo   = vec4(albedo, 1.0);
    gNormal   = vec4(N, clamp(uRoughness, 0.04, 1.0));
    gPosition = vec4(WorldPos, clamp(uMetallic, 0.0, 1.0));
}
