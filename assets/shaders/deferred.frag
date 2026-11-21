#version 330 core
// 延迟着色的光照 Pass：全屏三角，逐像素从 G-buffer 取数据算光照。
// 几何只光栅化一遍，光照只算真正被照到的像素。
layout (location = 0) out vec4 FragColor;

in vec2 vUV;

#define MAX_LIGHTS 8

// ---- G-buffer ----
uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gPosition;

// ---- 多光源（与 horror.frag 完全一致的布局，CPU 端一次填满） ----
uniform int   lightCount;
uniform vec3  lightPos[MAX_LIGHTS];
uniform vec3  lightDirArr[MAX_LIGHTS];
uniform vec3  lightColorArr[MAX_LIGHTS];
uniform float lightIntensity[MAX_LIGHTS];
uniform float lightCutoff[MAX_LIGHTS];
uniform float lightOuterCutoff[MAX_LIGHTS];
uniform int   lightType[MAX_LIGHTS];

uniform int uShadowMode;   // 0 = 单点，1 = PCF 3x3，2 = PCSS

uniform vec3 viewPos;

uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
uniform sampler2D shadowMap2;
uniform sampler2D shadowMap3;
uniform mat4 lightSpaceMatrices[MAX_LIGHTS];

uniform samplerCube shadowCube0;
uniform samplerCube shadowCube1;
uniform samplerCube shadowCube2;
uniform samplerCube shadowCube3;
uniform float pointFarPlane[MAX_LIGHTS];

// PBR 亮度补偿：Cook-Torrance 的漫反射带 1/PI，比 Blinn-Phong 暗约 3 倍。
// 3.14 保持和旧管线一致的观感，调到 1.0 就是纯物理值。
uniform float uPBRGain;

// ---- IBL（基于图像的环境光照）----
uniform samplerCube uIrradianceMap;   // 漫反射辐照度（低频）
uniform samplerCube uPrefilterMap;    // 镜面预过滤（mip = roughness）
uniform sampler2D   uBrdfLUT;         // split-sum 第二项
uniform int         uIBLEnabled;      // 0 = 回退到旧的常量环境光
uniform float       uEnvIntensity;    // 环境光倍率

const float PI = 3.14159265359;

// 预过滤 cubemap 一共 5 级 mip（0~4），最粗糙那级对应 roughness = 1
const float MAX_REFLECTION_LOD = 4.0;

float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap);
float PointShadowCalculation(vec3 fragPos, vec3 lightPos, samplerCube cube, float farPlane);
float DistributionGGX(float NdotH, float roughness);
float GeometrySmith(float NdotV, float NdotL, float roughness);
vec3  fresnelSchlick(float cosTheta, vec3 F0);
vec3  fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

void main()
{
    vec4 albedoSample = texture(gAlbedo, vUV);
    // a = 0 表示这个像素没有几何（天空），直接输出黑，不参与光照
    if (albedoSample.a < 0.5)
    {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec4 normalSample = texture(gNormal,   vUV);
    vec4 posSample    = texture(gPosition, vUV);

    vec3  albedo    = albedoSample.rgb;
    vec3  N         = normalize(normalSample.rgb);
    vec3  WorldPos  = posSample.rgb;
    vec3  V         = normalize(viewPos - WorldPos);

    // G-buffer 的 alpha 通道顺手存了材质参数
    float roughness = clamp(normalSample.a, 0.04, 1.0);
    float metallic  = clamp(posSample.a,    0.0,  1.0);

    // 电介质反射率固定 0.04，金属则用 albedo 当 F0
    vec3  F0    = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(N, V), 1e-4);

    // 环境光：IBL 开就采样烘焙好的三张图，关了就退回旧的常量
    vec3 ambient;
    if (uIBLEnabled == 1)
    {
        // 粗糙表面的菲涅尔：越粗糙反射越弱、越不集中
        vec3 F  = fresnelSchlickRoughness(NdotV, F0, roughness);
        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        // 漫反射：法线方向查低频辐照度图
        vec3 irradiance = texture(uIrradianceMap, N).rgb;
        vec3 diffuseIBL = irradiance * albedo;

        // 镜面：反射向量方向查预过滤图，mip 由 roughness 选
        vec3  R           = reflect(-V, N);
        vec3  prefiltered = textureLod(uPrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
        // BRDF LUT：横轴 NdotV，纵轴 roughness，返回 (A, B)
        vec2  brdf        = texture(uBrdfLUT, vec2(NdotV, roughness)).rg;
        vec3  specularIBL = prefiltered * (F * brdf.x + brdf.y);

        ambient = (kD * diffuseIBL + specularIBL) * uEnvIntensity;
    }
    else
    {
        // 极弱环境光，营造黑暗（金属没有漫反射，先按比例压掉）
        ambient = 0.03 * albedo * (1.0 - metallic);
    }

    vec3 result = ambient;

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (i >= lightCount) break;

        vec3  L;
        float attenuation = 1.0;
        float spot        = 1.0;

        if (lightType[i] == 2)
        {
            // 平行光
            L = normalize(-lightDirArr[i]);
            attenuation = 1.0f;
        }
        else
        {
            vec3  toLight = lightPos[i] - WorldPos;
            float dist    = length(toLight);
            L = toLight / max(dist, 1e-4);

            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

            if (lightType[i] == 1)
            {
                float theta = dot(L, normalize(-lightDirArr[i]));
                float eps   = max(lightCutoff[i] - lightOuterCutoff[i], 1e-4);
                float x     = clamp((theta - lightOuterCutoff[i]) / eps, 0.0, 1.0);
                // 中心亮圆、外圈渐暗：S 曲线让边缘平滑，再用指数压一下外围亮度
                spot = pow(x, 5.0);
                spot *= 1.0 + 0.2 * smoothstep(0.94, 1.0, x);
                attenuation = 1.0f;
            }
        }

        // ---- Cook-Torrance ----
        float NdotL = max(dot(N, L), 0.0);
        vec3  H     = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float HdotV = max(dot(H, V), 0.0);

        float D = DistributionGGX(NdotH, roughness);
        float G = GeometrySmith(NdotV, NdotL, roughness);
        vec3  F = fresnelSchlick(HdotV, F0);

        // 高光项；分母的 4*NdotV*NdotL 是微表面 BRDF 的归一化因子
        vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 1e-4);
        // 能量守恒：被反射掉的那部分 F 不能再当漫反射
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

        vec3 radiance = lightColorArr[i] * lightIntensity[i] * attenuation * spot;

        float occl = 1.0;
        if (lightType[i] == 1 || lightType[i] == 2)
        {
            vec4 lp = lightSpaceMatrices[i] * vec4(WorldPos, 1.0);
            float shadow = 0.0;
            if      (i == 0) shadow = ShadowCalculation(lp, shadowMap0);
            else if (i == 1) shadow = ShadowCalculation(lp, shadowMap1);
            else if (i == 2) shadow = ShadowCalculation(lp, shadowMap2);
            else if (i == 3) shadow = ShadowCalculation(lp, shadowMap3);
            occl = 1.0 - shadow;
        }
        else if (lightType[i] == 0)
        {
            float shadow = 0.0;
            if      (i == 0) shadow = PointShadowCalculation(WorldPos, lightPos[0], shadowCube0, pointFarPlane[0]);
            else if (i == 1) shadow = PointShadowCalculation(WorldPos, lightPos[1], shadowCube1, pointFarPlane[1]);
            else if (i == 2) shadow = PointShadowCalculation(WorldPos, lightPos[2], shadowCube2, pointFarPlane[2]);
            occl = 1.0 - shadow;
        }

        result += occl * (kD * albedo / PI + specular) * radiance * NdotL * uPBRGain;
    }

    // 距离雾：按到眼睛的距离，远处融进黑暗
    float fog = 1.0 - exp(-0.045 * length(viewPos - WorldPos));
    result = mix(result, vec3(0.0), fog);

    FragColor = vec4(result, 1.0);
}

// 黄金角螺旋（Vogel 分布）：单位圆内第 i 个采样点（共 n 个）
vec2 goldenDisk(int i, int n)
{
    float fi = float(i);
    float r  = sqrt((fi + 0.5) / float(n));
    float th = fi * 2.39996323;
    return vec2(r * cos(th), r * sin(th));
}

// 聚光、平行光阴影计算
float ShadowCalculation(vec4 fragPosLightSpace, sampler2D shadowMap)
{
    vec3 proj = fragPosLightSpace.xyz / fragPosLightSpace.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
    {
        return 0.0;
    }

    float currentDepth = proj.z;
    float bias         = 0.0001;
    vec2  texelSize    = 1.0 / vec2(textureSize(shadowMap, 0));

    if (uShadowMode == 0)
    {
        float closestDepth = texture(shadowMap, proj.xy).r;
        return (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
    }

    if (uShadowMode == 2)
    {
        const int   kSamples      = 16;
        const float kSearchTexels = 4.0;
        const float kLightSize    = 0.10;
        const float kMaxRadius    = 6.0;

        float blockerSum   = 0.0;
        int   blockerCount = 0;
        for (int i = 0; i < kSamples; ++i)
        {
            vec2  off = goldenDisk(i, kSamples) * texelSize * kSearchTexels;
            float d   = texture(shadowMap, proj.xy + off).r;
            if (d < currentDepth - bias)
            {
                blockerSum += d;
                blockerCount++;
            }
        }
        if (blockerCount == 0) return 0.0;

        float avgBlocker = blockerSum / float(blockerCount);
        float penumbra   = (currentDepth - avgBlocker) / max(avgBlocker, 1e-5) * kLightSize;
        float radius     = clamp(penumbra / texelSize.x, 1.0, kMaxRadius);

        float shadow = 0.0;
        for (int i = 0; i < kSamples; ++i)
        {
            vec2  off = goldenDisk(i, kSamples) * texelSize * radius;
            float d   = texture(shadowMap, proj.xy + off).r;
            shadow += (d < currentDepth - bias) ? 1.0 : 0.0;
        }
        return shadow / float(kSamples);
    }

    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float closestDepth = texture(shadowMap, proj.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

// 点光源阴影：cubemap 里存的是 length(fragPos - lightPos) / farPlane
float PointShadowCalculation(vec3 fragPos, vec3 lightPos, samplerCube cube, float farPlane)
{
    vec3  dir     = fragPos - lightPos;
    float current = length(dir) / farPlane;
    float bias    = 0.015;

    const vec3 kOffsets[20] = vec3[](
        vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
        vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
        vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
        vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
        vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );

    float texelAngle = 25.0 / float(textureSize(cube, 0).x);
    float diskRadius = texelAngle * 1.5;

    float shadow = 0.0;
    for (int i = 0; i < 20; ++i)
    {
        float closest = texture(cube, dir + kOffsets[i] * diskRadius).r;
        shadow += (current - bias > closest) ? 1.0 : 0.0;
    }
    return shadow / 20.0;
}

// ---- PBR 的四个核心函数 ----

// 法线分布函数（GGX / Trowbridge-Reitz）：
// 给定半程向量与法线的夹角，返回这个微表面的朝向概率密度
float DistributionGGX(float NdotH, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float denom  = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-6);
}

// 几何遮蔽（Smith + Schlick-GGX）：微表面互相遮挡和阴影的占比
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// 菲涅尔（Schlick 近似）：视线越斜，反射占比越高
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// IBL 版菲涅尔：粗糙表面的反射率上限随粗糙度降低
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
             * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
