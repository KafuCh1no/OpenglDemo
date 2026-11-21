#version 330 core
// 镜面反射预过滤：按 roughness 把环境光做 GGX 重要性采样模糊，存进 cubemap 的 mip 链。
// mip 0 = 完全清晰的镜面，mip N = 最粗糙的漫反射级反射。
out vec4 FragColor;

in vec3 vLocalPos;

uniform samplerCube uEnvMap;
uniform float       uRoughness;

const float PI = 3.14159265359;

// 把整数 i 的位倒过来，得到低差异序列
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;    // / 2^32
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// 按 GGX 分布采样一个半程向量 H
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a       = roughness * roughness;
    float phi     = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    // 切空间 -> 世界空间
    vec3 up    = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float DistributionGGX(float NdotH, float roughness)
{
    float a     = roughness * roughness;
    float a2    = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 1e-6);
}

void main()
{
    vec3 N = normalize(vLocalPos);
    // 预过滤时假设视线方向 = 法线方向（近似，标准做法）
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 256u;
    vec3  prefilteredColor = vec3(0.0);
    float totalWeight      = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, uRoughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            // 根据 PDF 决定该采哪一级 mip：粗糙度越大采越模糊的 mip，抑制噪点
            float D     = DistributionGGX(max(dot(N, H), 0.0), uRoughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf   = D * NdotH / (4.0 * HdotV) + 0.0001;

            float saTexel  = 4.0 * PI / (6.0 * 512.0 * 512.0);
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            float mipLevel = uRoughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

            prefilteredColor += textureLod(uEnvMap, L, mipLevel).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 1e-4);
    FragColor = vec4(prefilteredColor, 1.0);
}
